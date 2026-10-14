#include "loader.h"

#include <elf.h>
#ifndef R_X86_64_JUMP_SLOT
#define R_X86_64_JUMP_SLOT R_X86_64_JMP_SLOT
#endif
#include <sys/mman.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <vector>

extern "C" void __register_frame(const void *);
extern "C" void __deregister_frame(const void *);

namespace game_module {
namespace {
constexpr size_t limit = 256 * 1024 * 1024;

bool inside(uint64_t start, uint64_t length, uint64_t total) {
    return start <= total && length <= total - start;
}

template<class T> T read(const std::vector<char> &bytes, size_t offset) {
    if (!inside(offset, sizeof(T), bytes.size())) throw std::runtime_error("Truncated game module");
    T value;
    memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}

const char *name_at(const std::string &strings, uint32_t offset) {
    if (offset >= strings.size() || !memchr(strings.data() + offset, 0, strings.size() - offset))
        throw std::runtime_error("Invalid game module symbol name");
    return strings.data() + offset;
}
}

Image::~Image() {
    for (const void *frame : frames_) __deregister_frame(frame);
    if (memory_) munmap(memory_, size_);
}

void *Image::symbol(const char *name, size_t minimum_size) const {
    if (!memory_) return nullptr;
    for (size_t i = 0; i < symbols_.size(); i += sizeof(Elf64_Sym)) {
        Elf64_Sym sym;
        memcpy(&sym, symbols_.data() + i, sizeof(sym));
        if (sym.st_shndx != SHN_UNDEF && sym.st_shndx != SHN_ABS &&
            !strcmp(name_at(strings_, sym.st_name), name) && sym.st_size >= minimum_size &&
            inside(sym.st_value, sym.st_size, size_))
            return static_cast<char *>(memory_) + sym.st_value;
    }
    return nullptr;
}

bool Image::load(const char *path, Resolver resolve, std::string &error) {
    if (memory_) { error = "A game module is already loaded"; return false; }
    void *mapping = MAP_FAILED;
    size_t span = 0;
    try {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) throw std::runtime_error("Game module missing; run setup on a computer, then copy its private package");
        auto length = file.tellg();
        if (length < static_cast<std::streamoff>(sizeof(Elf64_Ehdr)) ||
            length > static_cast<std::streamoff>(limit))
            throw std::runtime_error("Invalid game module size");
        std::vector<char> bytes(static_cast<size_t>(length));
        file.seekg(0);
        file.read(bytes.data(), bytes.size());
        if (!file) throw std::runtime_error("Cannot read game module");

        const auto eh = read<Elf64_Ehdr>(bytes, 0);
        if (memcmp(eh.e_ident, ELFMAG, SELFMAG) || eh.e_ident[EI_CLASS] != ELFCLASS64 ||
            eh.e_ident[EI_DATA] != ELFDATA2LSB || eh.e_ident[EI_VERSION] != EV_CURRENT ||
            eh.e_type != ET_DYN || eh.e_machine != EM_X86_64 || eh.e_version != EV_CURRENT ||
            eh.e_phentsize != sizeof(Elf64_Phdr) || eh.e_shentsize != sizeof(Elf64_Shdr) ||
            !eh.e_phnum || !eh.e_shnum ||
            !inside(eh.e_phoff, uint64_t(eh.e_phnum) * sizeof(Elf64_Phdr), bytes.size()) ||
            !inside(eh.e_shoff, uint64_t(eh.e_shnum) * sizeof(Elf64_Shdr), bytes.size()))
            throw std::runtime_error("Unsupported game module format");

        const auto page = static_cast<size_t>(sysconf(_SC_PAGESIZE));
        if (!page || page > 65536 || (page & (page - 1))) throw std::runtime_error("Invalid page size");
        std::vector<Elf64_Phdr> segments;
        for (unsigned i = 0; i < eh.e_phnum; ++i) {
            auto ph = read<Elf64_Phdr>(bytes, eh.e_phoff + i * sizeof(Elf64_Phdr));
            if (ph.p_type == PT_TLS) throw std::runtime_error("Game module must not contain TLS");
            if (ph.p_type != PT_LOAD) continue;
            if (ph.p_filesz > ph.p_memsz || !inside(ph.p_offset, ph.p_filesz, bytes.size()) ||
                !inside(ph.p_vaddr, ph.p_memsz, limit) || (ph.p_flags & ~(PF_R | PF_W | PF_X)))
                throw std::runtime_error("Invalid game module segment");
            for (const auto &old : segments)
                if (ph.p_vaddr < old.p_vaddr + old.p_memsz && old.p_vaddr < ph.p_vaddr + ph.p_memsz)
                    throw std::runtime_error("Overlapping game module segments");
            segments.push_back(ph);
            span = std::max(span, static_cast<size_t>(ph.p_vaddr + ph.p_memsz));
        }
        if (!span) throw std::runtime_error("Empty game module");
        span = (span + page - 1) & ~(page - 1);
        mapping = mmap(nullptr, span, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
        if (mapping == MAP_FAILED) throw std::runtime_error("Cannot allocate game module memory");
        auto *base = static_cast<char *>(mapping);
        for (const auto &ph : segments) memcpy(base + ph.p_vaddr, bytes.data() + ph.p_offset, ph.p_filesz);

        auto mapped = [&](uint64_t offset, uint64_t size, bool writable = false) {
            for (const auto &ph : segments)
                if ((!writable || (ph.p_flags & PF_W)) && offset >= ph.p_vaddr &&
                    inside(offset - ph.p_vaddr, size, ph.p_memsz)) return true;
            return false;
        };

        std::vector<Elf64_Shdr> sections;
        int dynsym = -1;
        for (unsigned i = 0; i < eh.e_shnum; ++i) {
            auto sh = read<Elf64_Shdr>(bytes, eh.e_shoff + i * sizeof(Elf64_Shdr));
            if (sh.sh_type != SHT_NOBITS && !inside(sh.sh_offset, sh.sh_size, bytes.size()))
                throw std::runtime_error("Invalid game module section");
            if (sh.sh_type == SHT_DYNSYM) {
                if (dynsym != -1) throw std::runtime_error("Multiple game module symbol tables");
                dynsym = static_cast<int>(i);
            }
            if ((sh.sh_flags & SHF_ALLOC) && sh.sh_size &&
                (sh.sh_type == SHT_INIT_ARRAY || sh.sh_type == SHT_FINI_ARRAY || sh.sh_type == SHT_REL))
                throw std::runtime_error("Unsupported game module initialization or relocation");
            sections.push_back(sh);
        }
        if (dynsym < 0) throw std::runtime_error("Missing game module symbols");
        auto syms = sections[dynsym];
        if (syms.sh_entsize != sizeof(Elf64_Sym) || syms.sh_size % sizeof(Elf64_Sym) || syms.sh_link >= sections.size())
            throw std::runtime_error("Invalid game module symbol table");
        auto names = sections[syms.sh_link];
        if (names.sh_type != SHT_STRTAB) throw std::runtime_error("Invalid game module string table");
        std::string strings(bytes.data() + names.sh_offset, names.sh_size);
        std::vector<uintptr_t> addresses;
        for (size_t off = 0; off < syms.sh_size; off += sizeof(Elf64_Sym)) {
            auto sym = read<Elf64_Sym>(bytes, syms.sh_offset + off);
            const char *name = name_at(strings, sym.st_name);
            if (ELF64_ST_TYPE(sym.st_info) == STT_TLS || ELF64_ST_TYPE(sym.st_info) == STT_GNU_IFUNC)
                throw std::runtime_error("Unsupported game module symbol type");
            void *host = (sym.st_shndx == SHN_UNDEF || ELF64_ST_BIND(sym.st_info) == STB_WEAK) && *name ? resolve(name) : nullptr;
            if (host) addresses.push_back(reinterpret_cast<uintptr_t>(host));
            else if (sym.st_shndx == SHN_UNDEF) {
                if (*name && ELF64_ST_BIND(sym.st_info) != STB_WEAK)
                    throw std::runtime_error(std::string("Missing runtime symbol: ") + name);
                addresses.push_back(0);
            } else if (sym.st_shndx < sections.size() && mapped(sym.st_value, std::max<uint64_t>(sym.st_size, 1)))
                addresses.push_back(reinterpret_cast<uintptr_t>(base + sym.st_value));
            else throw std::runtime_error("Invalid game module symbol address");
        }

        for (const auto &sh : sections) {
            if (sh.sh_type != SHT_RELA || !(sh.sh_flags & SHF_ALLOC)) continue;
            if (sh.sh_link != static_cast<unsigned>(dynsym) || sh.sh_entsize != sizeof(Elf64_Rela) ||
                sh.sh_size % sizeof(Elf64_Rela)) throw std::runtime_error("Invalid game module relocation table");
            for (size_t off = 0; off < sh.sh_size; off += sizeof(Elf64_Rela)) {
                auto rel = read<Elf64_Rela>(bytes, sh.sh_offset + off);
                auto type = ELF64_R_TYPE(rel.r_info), index = ELF64_R_SYM(rel.r_info);
                if (type == R_X86_64_NONE) continue;
                if (!mapped(rel.r_offset, 8, true) || index >= addresses.size())
                    throw std::runtime_error("Invalid game module relocation target");
                uint64_t value;
                if (type == R_X86_64_RELATIVE) {
                    if (index || rel.r_addend < 0 || !mapped(rel.r_addend, 1))
                        throw std::runtime_error("Invalid relative game module relocation");
                    value = reinterpret_cast<uintptr_t>(base) + rel.r_addend;
                } else if (type == R_X86_64_64) value = addresses[index] + rel.r_addend;
                else if (type == R_X86_64_GLOB_DAT || type == R_X86_64_JUMP_SLOT) value = addresses[index];
                else throw std::runtime_error("Unsupported game module relocation type: " + std::to_string(type));
                memcpy(base + rel.r_offset, &value, 8);
            }
        }

        std::vector<int> permissions(span / page, 0);
        for (const auto &ph : segments) {
            int bits = ((ph.p_flags & PF_R) ? PROT_READ : 0) | ((ph.p_flags & PF_W) ? PROT_WRITE : 0) |
                       ((ph.p_flags & PF_X) ? PROT_EXEC : 0);
            for (size_t i = ph.p_vaddr / page; i < (ph.p_vaddr + ph.p_memsz + page - 1) / page; ++i)
                permissions[i] |= bits;
        }
        for (size_t i = 0; i < permissions.size(); ++i) {
            if ((permissions[i] & (PROT_WRITE | PROT_EXEC)) == (PROT_WRITE | PROT_EXEC))
                throw std::runtime_error("Game module segments share a writable executable page");
            if (mprotect(base + i * page, page, permissions[i]))
                throw std::runtime_error(std::string("Cannot protect game module memory: ") + strerror(errno));
        }

        if (eh.e_shstrndx >= sections.size()) throw std::runtime_error("Invalid game module section-name table");
        auto shnames = sections[eh.e_shstrndx];
        if (shnames.sh_type != SHT_STRTAB) throw std::runtime_error("Invalid game module section names");
        std::string section_names(bytes.data() + shnames.sh_offset, shnames.sh_size);
        for (const auto &sh : sections) {
            if (strcmp(name_at(section_names, sh.sh_name), ".eh_frame")) continue;
            if (!mapped(sh.sh_addr, sh.sh_size) || sh.sh_size < 4) throw std::runtime_error("Invalid game module unwind section");
            std::vector<const void *> records;
            size_t offset = 0;
            bool terminated = false;
            while (offset + 4 <= sh.sh_size) {
                uint32_t length;
                memcpy(&length, base + sh.sh_addr + offset, 4);
                if (!length) { terminated = true; break; }
                if (length < 4 || length == UINT32_MAX || !inside(offset + 4, length, sh.sh_size))
                    throw std::runtime_error("Invalid game module unwind record");
                uint32_t cie;
                memcpy(&cie, base + sh.sh_addr + offset + 4, 4);
                if (cie) {
                    if (cie > offset + 4) throw std::runtime_error("Invalid game module unwind CIE reference");
                    records.push_back(base + sh.sh_addr + offset);
                }
                offset += 4 + length;
            }
            if (!terminated) throw std::runtime_error("Unterminated game module unwind section");
#ifdef __PROSPERO__
            frames_ = std::move(records);
#else
            frames_.push_back(base + sh.sh_addr);
#endif
        }
        symbols_.assign(bytes.data() + syms.sh_offset, syms.sh_size);
        strings_ = std::move(strings);
        for (const void *frame : frames_) __register_frame(frame);
        memory_ = mapping;
        size_ = span;
        return true;
    } catch (const std::exception &ex) {
        frames_.clear();
        if (mapping != MAP_FAILED) munmap(mapping, span);
        error = ex.what();
        return false;
    }
}
}
