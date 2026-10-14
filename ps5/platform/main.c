int la_game_main(int argc, char **argv);
int main(void) {
    char exe[] = "/app0/eboot.bin";
    char root[] = "/app0/extracted";
    char *args[] = {exe, root, 0};
    return la_game_main(2, args);
}
