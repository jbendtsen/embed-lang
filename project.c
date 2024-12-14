#include "embed.h"

#include <stdio.h>
#include <stdlib.h>

void run_project(Project *p);

int main(int argc, char **argv)
{
    if (argc != 2) {
        printf(
            "embed-lang compiler\n"
            "usage: %s <main source file>\n", argv[0]
        );
        return 1;
    }

    FILE *f = fopen(argv[1], "rb");
    if (!f) {
        printf("Could not open \"%s\"\n", argv[1]);
        return 2;
    }

    fseek(f, 0, SEEK_END);
    int sz = ftell(f);
    rewind(f);

    if (sz < 1) {
        printf("\"%s\" was empty or could not be read\n", argv[1]);
        return 3;
    }

    u8 *buf = malloc(sz);
    fread(buf, 1, sz, f);
    fclose(f);

    Project project = {0};

    int first_buffer = ALLOC_STRUCT(&project.buffers, Buffer);
    *STRUCT_AT_POS(project.buffers, Buffer, first_buffer) = (Buffer) { .path = argv[1], .buf = buf, .size = sz };

    int first_ast = ALLOC_STRUCT(&project.asts, Ast);

    IntVector ac = {0};

    parse_source_file(
        STRUCT_AT_POS(project.asts, Ast, first_ast),
        STRUCT_AT_POS(project.buffers, Buffer, first_buffer),
        first_buffer,
        &ac
    );

    run_project(&project, &ac);
    //close_project(&project);

    return 0;
}

void run_project(Project *p) {

}
