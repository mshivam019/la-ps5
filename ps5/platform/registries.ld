/* Runtime imports and reboot callbacks must survive --gc-sections. */
SECTIONS {
    .data.rel.ro.hle_impls : ALIGN(8) {
        HIDDEN(__start_hle_impls = .);
        KEEP(*(hle_impls))
        HIDDEN(__stop_hle_impls = .);
    } : relro
    .data.rel.ro.resets : ALIGN(8) {
        HIDDEN(__start_resets = .);
        KEEP(*(resets))
        HIDDEN(__stop_resets = .);
    } : relro
    .data.rel.ro.services : ALIGN(8) {
        HIDDEN(__start_services = .);
        KEEP(*(services))
        HIDDEN(__stop_services = .);
    } : relro
} INSERT AFTER .data.rel.ro;
