/*  A program to simulate keyboard and mouse inputs
    Copyright (C) 2025 Anonymous1212144

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

typedef struct
{
    int opcode;
    uintmax_t immediate;
} instruction;

typedef struct
{
    INPUT *input;
    size_t len;
} sequence;

typedef struct
{
    void *data;
    size_t len;
    size_t cap;
    size_t unit;
} vector;

enum
{
    instruction_size = sizeof(instruction),
    sequence_size = sizeof(sequence),
    input_obj_size = sizeof(INPUT),
    sizet_size = sizeof(size_t),
    uintmax_size = sizeof(uintmax_t),
};

vector inputs = {NULL, 0, 0, sequence_size};
vector codes = {NULL, 0, 0, instruction_size};
int crash = 0;
uintmax_t *memory;

void handle_error(const char *const prompt)
{
    perror(prompt);
    exit(EXIT_FAILURE);
}

void expand(vector *v)
{
    if (v->len == v->cap)
    {
        if (!v->cap)
            v->cap++;
        else
        {
            v->cap <<= 1;
            if (!v->cap)
                v->cap--;
        }
        v->data = realloc(v->data, v->cap * v->unit);
        if (!v->data)
            handle_error("Error compiling");
    }
}

char *put_num(char *buffer, const uintmax_t num)
{
    if (num >= 10)
        buffer = put_num(buffer, num / 10);
    *buffer = '0' + num % 10;
    return buffer + 1;
}

void print_num(const char *const prefix, const char *const suffix, const size_t prefix_len, const size_t suffix_len, const uintmax_t num)
{
    char *buffer = malloc(prefix_len + suffix_len + uintmax_size - 1);
    memcpy(buffer, prefix, prefix_len - 1);
    memcpy(put_num(buffer + prefix_len - 1, num), suffix, suffix_len);
    puts(buffer);
    free(buffer);
}

void print_msg(const char *const prefix, const char *const suffix, const size_t prefix_len, const size_t suffix_len, const char c, const size_t index)
{
    char *buffer = malloc(prefix_len + suffix_len - 1);
    memcpy(buffer, prefix, prefix_len - 1);
    if (c)
        buffer[index] = c;
    memcpy(buffer + prefix_len - 1, suffix, suffix_len);
    puts(buffer);
    free(buffer);
}

void error(const char *const prompt, const size_t prompt_len, const char c)
{
    print_msg("Error reading \"c\": ", prompt, 20, prompt_len, c, 15);
    exit(EXIT_FAILURE);
}

void warn(const char *const prompt, const size_t prompt_len, const char c)
{
    if (c)
        print_msg("Warning for character \"c\": ", prompt, 28, prompt_len, c, 23);
    else
        print_msg("Warning for EOF: ", prompt, 18, prompt_len, 0, 0);
    if (crash)
        crash++;
}

size_t read(const char *const chars, const int char_set)
{
    // 0 for all, 1 for 0-9, 2 for numpad, 3 for 0-9a-fA-F
    int first = 1;
    const char *ptr = chars;
    int exit = 0;
    for (; *ptr != '\n' && *ptr != '\r'; ptr++)
    {
        switch (char_set)
        {
        case 1:
            if ((*ptr < 48 && !(first && *ptr == 45)) || *ptr > 57)
                exit = 1;
            break;
        case 2:
            if (*ptr < 42 || *ptr > 57)
                exit = 1;
            break;
        case 3:
            if (*ptr < 71)
            {
                if (*ptr < 58)
                {
                    if (*ptr < 48)
                        exit = 1;
                }
                else if (*ptr < 65)
                    exit = 1;
            }
            else if (*ptr < 103)
            {
                if (*ptr < 97)
                    exit = 1;
            }
            else
                exit = 1;
            break;
        }
        if (exit)
            break;
        first = 0;
    }
    if (!(ptr - chars))
        warn("Nothing read", 13, *(ptr - 1));
    return ptr - chars;
}

uintmax_t parse_num(const char *chars, const size_t len, const int type)
{
    // type is 0 for base10 and 1 for base16
    uintmax_t output = 0;
    int cof = 1;
    if (type)
    {
        for (size_t i = 0; i < len; i++, chars++)
        {
            output <<= 4;
            char c = *chars;
            if (c < 58)
                output |= c - 48;
            else if (c < 91)
                output |= c - 55;
            else
                output |= c - 87;
        }
    }
    else
    {
        for (size_t i = 0; i < len; i++, chars++)
        {
            output *= 10;
            char c = *chars;
            if (c == '-')
                cof = -1;
            else
                output += c - 48;
        }
    }
    return cof * output;
}

void add_code(const int opcode, const uintmax_t immediate)
{
    expand(&codes);
    ((instruction *)codes.data)[codes.len].opcode = opcode;
    ((instruction *)codes.data)[codes.len].immediate = immediate;
    codes.len++;
}

void add_event(const int type, const uintmax_t *const data)
{
    static size_t mem_needed = 0;

    static vector loop = {NULL, 0, 0, sizet_size};
    static vector stack = {NULL, 0, 0, input_obj_size};
    static vector repeats = {NULL, 0, 0, sizet_size};
    uintmax_t num_repeat = 0;

    static int group_state = 9;
    static uintmax_t sleep = 0;

    switch (type)
    {
    case 0:
        // begin loop
        if (group_state & 4)
        {
            warn("Missing \")\", added automatically", 33, '}');
            add_event(5, (uintmax_t[]){1});
        }
        while (repeats.len)
        {
            warn("Missing \"]\", added automatically", 33, '}');
            add_event(6, (uintmax_t[]){1});
        }

        expand(&loop);
        if (++loop.len > mem_needed)
            mem_needed = loop.len;
        ((size_t *)loop.data)[loop.len - 1] = codes.len;
        if (!data[0])
        {
            warn("Invalid loop count, assuming loop of 1", 39, '{');
            add_code(type, 1);
        }
        else
            add_code(type, data[0]);
        break;
    case 1:
        // end loop
        if (group_state & 4)
        {
            warn("Missing \")\", added automatically", 33, '}');
            add_event(5, (uintmax_t[]){1});
        }
        while (repeats.len)
        {
            warn("Missing \")\", added automatically", 33, '}');
            add_event(6, (uintmax_t[]){1});
        }

        loop.len--;
        add_code(type, ((size_t *)loop.data)[loop.len]);
        break;
    case 2:
        // execute
        if (data[0] && group_state & 4)
        {
            warn("Keyboard commands for mouse, ignored", 37, '(');
            break;
        }
        if (group_state & 1)
        {
            if (group_state & 8)
            {
                stack.data = calloc(1, stack.unit);
                if (!stack.data)
                    handle_error("Error compiling");
                stack.len = 0;
                stack.cap = 1;
                group_state |= 16;
                group_state &= ~8;
            }
            size_t old_cap = stack.cap;
            expand(&stack);
            if (old_cap != stack.cap)
                memset(&((INPUT *)stack.data)[old_cap], 0, (stack.cap - old_cap) * stack.unit);
            group_state |= 2;
            group_state &= ~1;
        }

        INPUT *curr = &((INPUT *)stack.data)[stack.len];

        if (data[0])
        {
            curr->type = INPUT_KEYBOARD;
            curr->ki.wVk = data[1];
            if (data[2])
                curr->ki.dwFlags = KEYEVENTF_KEYUP;
        }
        else
        {
            curr->mi.dwFlags |= data[1];
            if (data[1] == MOUSEEVENTF_WHEEL)
            {
                curr->mi.mouseData = data[2];
            }
            else if (data[1] == MOUSEEVENTF_MOVE)
            {
                curr->mi.dx = data[2];
                curr->mi.dy = data[3];
                if (data[4])
                    curr->mi.dwFlags |= MOUSEEVENTF_ABSOLUTE;
            }
        }
        break;
    case 3:
        // sleep
        if (repeats.len)
            warn("Sleep command in arrayed inputs, ignored", 41, 's');
        else
            add_code(type, data[0]);
        break;
    case 4:
        // repeated sleep
        sleep = data[0];
        break;
    case 5:
        // simultaneous mouse events
        if (data[0])
        {
            if (!(group_state & 2))
                warn("No instructions", 16, ')');
            if (group_state & 4)
                group_state &= ~4;
            else
                warn("Mismatched brackets, ignored", 29, ')');
        }
        else
        {
            if (group_state & 4)
                warn("Nested brackets, ignored", 25, '(');
            else
                group_state |= 4;
        }
        break;
    case 6:
        // repeated/copied and arrayed events
        if (data[0])
        {
            if (!repeats.len)
            {
                warn("Mismatched brackets, ignored", 29, ']');
                break;
            }
            if (group_state & 4)
            {
                warn("Missing \")\", added automatically", 33, ']');
                add_event(5, (uintmax_t[]){1});
            }
            if (!(num_repeat = data[1]))
                warn("Invalid copy count, assuming copy of 1", 39, ']');
            repeats.len--;
            if (!repeats.len)
                group_state &= ~32;

            size_t i0 = ((size_t *)repeats.data)[repeats.len];
            size_t si = stack.len - i0;
            if (!si)
            {
                warn("No instructions", 16, ']');
                break;
            }
            if (num_repeat > 1)
            {
                size_t i1 = si * num_repeat + i0;
                size_t size = si * stack.unit;
                size_t dlen = si * num_repeat * stack.unit;

                if (((size_t)-1) / size < num_repeat)
                    error("Too many instructions", 23, ']');
                stack.data = realloc(stack.data, i1 * stack.unit);
                if (!stack.data)
                    handle_error("Error compiling");

                char *start = (char *)stack.data + i0 * stack.unit;
                char *index = (char *)stack.data + stack.len * stack.unit;

                while (1)
                {
                    memcpy(index, start, size);
                    index += size;
                    size <<= 1;
                    if ((size << 1) > dlen)
                    {
                        if (dlen - size)
                            memcpy(index, start, dlen - size);
                        break;
                    }
                }
                stack.len = i1;
                stack.cap = i1;
            }
        }
        else
        {
            if (group_state & 4)
            {
                warn("Missing \")\", added automatically", 33, ']');
                add_event(5, (uintmax_t[]){1});
            }

            expand(&repeats);
            if (!repeats.len)
                stack.len = 0;
            ((size_t *)repeats.data)[repeats.len] = stack.len;
            repeats.len++;
            group_state |= 32;
        }
        break;
    case 7:
        // finalize
        if (group_state & 4)
        {
            warn("Missing \")\", added automatically", 33, 0);
            add_event(5, (uintmax_t[]){1});
        }
        while (repeats.len)
        {
            warn("Missing \"]\", added automatically", 33, 0);
            add_event(6, (uintmax_t[]){1, 0});
        }
        while (loop.len--)
        {
            warn("Missing \"}\", added automatically", 33, 0);
            add_code(1, ((size_t *)loop.data)[loop.len]);
        }

        free(loop.data);
        free(repeats.data);
        memory = malloc(mem_needed * uintmax_size);
        return;
    }

    if (!(group_state & 4))
    {
        if ((group_state & 6) == 2)
        {
            stack.len++;
            group_state &= ~2;
            group_state |= 1;
        }
        if ((group_state & 48) == 16)
        {
            expand(&inputs);
            ((sequence *)inputs.data)[inputs.len].input = stack.data;
            ((sequence *)inputs.data)[inputs.len].len = stack.len;
            add_code(2, inputs.len);
            if (sleep)
                add_code(3, sleep);
            inputs.len++;
            group_state &= ~16;
            group_state |= 8;
        }
    }
}

void parse_keys(const char *chars, const size_t len, const HKL layout)
{
    int state = 0;
    for (size_t i = 0; i < len; i++, chars++)
    {
        int key = VkKeyScanExA(*chars, layout);
        if (key == 0xFFFF)
        {
            warn("Invalid key, ignored", 21, *chars);
            continue;
        }
        int state_new = (key >> 8) & 255;
        int state2 = state_new ^ state;
        char low = key & 255;
        if (state2)
        {
            if (state2 & 1)
                add_event(2, (uintmax_t[]){1, VK_SHIFT, state & 1});
            if (state2 & 2)
                add_event(2, (uintmax_t[]){1, VK_CONTROL, state & 2});
            if (state2 & 4)
                add_event(2, (uintmax_t[]){1, VK_MENU, state & 4});
            if (state2 >> 3)
                warn("Keys other than shift, control, or alt required, skipped those keys", 68, *chars);
        }
        add_event(2, (uintmax_t[]){1, low, 0});
        add_event(2, (uintmax_t[]){1, low, 1});
        state = state_new;
    }
    if (state)
    {
        if (state & 1)
            add_event(2, (uintmax_t[]){1, VK_SHIFT, 1});
        if (state & 2)
            add_event(2, (uintmax_t[]){1, VK_CONTROL, 1});
        if (state & 4)
            add_event(2, (uintmax_t[]){1, VK_MENU, 1});
    }
}

void compile(const char *const code, const size_t chars)
{
    static HKL layout = NULL;
    static const char words[] = "SswPpLlMmRrnkKCc()[]{}";
    static const int mouse[] = {MOUSEEVENTF_LEFTDOWN, MOUSEEVENTF_LEFTUP, MOUSEEVENTF_MIDDLEDOWN, MOUSEEVENTF_MIDDLEUP, MOUSEEVENTF_RIGHTDOWN, MOUSEEVENTF_RIGHTUP};
    const char *ptr = code;
    while (ptr - code < chars)
    {
        char c = *(ptr++);
        if (c == '\n' || c == '\r')
            continue;
        int state;
        for (state = 0; c != words[state] && state < 22; state++)
            ;
        if (state == 22)
        {
            warn("Unknown command, ignored", 25, c);
            continue;
        }
        if (state < 11)
        {
            if (state < 5)
            {
                size_t read_len = read(ptr, 1);
                uintmax_t num = parse_num(ptr, read_len, 0);
                ptr += read_len;
                switch (state)
                {
                case 0:
                    add_event(4, (uintmax_t[]){num});
                    break;
                case 1:
                    add_event(3, (uintmax_t[]){num});
                    break;
                case 2:
                    add_event(2, (uintmax_t[]){0, MOUSEEVENTF_WHEEL, num});
                    break;
                default:
                {
                    ptr++;
                    read_len = read(ptr, 1);
                    uintmax_t num2 = parse_num(ptr, read_len, 0);
                    ptr += read_len;
                    add_event(2, (uintmax_t[]){0, MOUSEEVENTF_MOVE, num, num2, state == 3});
                    break;
                }
                }
            }
            else
                add_event(2, (uintmax_t[]){0, mouse[state - 5]});
        }
        else
        {
            if (state < 16)
            {
                if (state == 11)
                {
                    size_t read_len = read(ptr, 2);
                    for (size_t j = 0; j < read_len; j++, ptr++)
                    {
                        int key = *ptr;
                        if (key < 48)
                            key = key - 42 + VK_MULTIPLY;
                        else
                            key = key - 48 + VK_NUMPAD0;
                        add_event(2, (uintmax_t[]){1, *ptr - 48 + VK_NUMPAD0, 0});
                        add_event(2, (uintmax_t[]){1, *ptr - 48 + VK_NUMPAD0, 1});
                    }
                }
                else if (state == 12)
                {
                    size_t read_len = read(ptr, 0);
                    if (!layout)
                        layout = LoadKeyboardLayoutA("00000409", 0);
                    parse_keys(ptr, read_len, layout);
                    ptr += read_len;
                }
                else
                {
                    size_t read_len = read(ptr, 3);
                    if (state == 13)
                    {
                        char *slice = malloc(read_len + 1);
                        memcpy(slice, ptr, read_len);
                        slice[read_len] = 0;
                        layout = LoadKeyboardLayoutA(slice, 0);
                        free(slice);
                        ptr += read_len;
                    }
                    else
                    {
                        if (read_len & 1)
                        {
                            add_event(2, (uintmax_t[]){1, parse_num(ptr, 1, 1), state == 15});
                            ptr++;
                        }
                        read_len >>= 1;
                        for (size_t j = 0; j < read_len; j++, ptr += 2)
                            add_event(2, (uintmax_t[]){1, parse_num(ptr, 2, 1), state == 15});
                    }
                }
            }
            else
            {
                if (state == 16)
                    add_event(5, (uintmax_t[]){0});
                else if (state == 17)
                    add_event(5, (uintmax_t[]){1});
                else if (state == 18)
                    add_event(6, (uintmax_t[]){0});
                else if (state == 19)
                {
                    size_t read_len = read(ptr, 1);
                    uintmax_t num = parse_num(ptr, read_len, 0);
                    ptr += read_len;
                    add_event(6, (uintmax_t[]){1, num});
                }
                else if (state == 20)
                {
                    size_t read_len = read(ptr, 1);
                    uintmax_t num = parse_num(ptr, read_len, 0);
                    ptr += read_len;
                    add_event(0, (uintmax_t[]){num});
                }
                else
                    add_event(1, NULL);
            }
        }
    }
    add_event(7, NULL);
}

void execute()
{
    sequence *seq = (sequence *)inputs.data;
    instruction *ins = (instruction *)codes.data;
    size_t m = -1;
    for (size_t i = 0; i < codes.len; i++)
    {
        switch (ins[i].opcode)
        {
        case 0:
            memory[++m] = ins[i].immediate;
            break;
        case 1:
            if (--memory[m])
                i = ins[i].immediate;
            else
                m--;
            break;
        case 2:
            sequence input = seq[ins[i].immediate];
            if (SendInput(input.len, input.input, input_obj_size) != input.len)
                puts("Warning: some inputs failed to send");
            break;
        case 3:
            Sleep(ins[i].immediate);
            break;
        default:
            puts("Error: Internal error while executing, please submit the input file with a bug report");
            exit(EXIT_FAILURE);
            break;
        }
    }
}

int main(const int argc, const char **const argv)
{
    if (argc < 2)
    {
        puts("No file entered, defaulting to \"keys.txt\"");
        argv[1] = "keys.txt";
    }
    const char *flag = "-W";
    int unknowns = 0;
    for (int i = 2; i < argc; i++)
    {
        size_t j = 0;
        int match = 0;
        char c;
        while ((c = argv[i][j]))
        {
            if (c == flag[j])
                match++;
            else
                break;
            j++;
        }
        if (match == 2)
        {
            puts("Enabled error for warnings");
            crash = 1;
        }
        else
            unknowns++;
    }
    if (unknowns)
        print_num("Ignored ", " unknown flags", 9, 15, unknowns);

    FILE *file = fopen(argv[1], "rb");
    if (file == NULL)
        handle_error("Error opening file");
    size_t keys_file_len;
    fseek(file, 0, SEEK_END);
    keys_file_len = ftell(file);
    rewind(file);

    // Allocate memory to store the file content
    char *keys_file = malloc(keys_file_len + 1);
    if (!keys_file)
        handle_error("Error loading file");

    // Read the file into the buffer
    fread(keys_file, 1, keys_file_len, file);
    keys_file[keys_file_len] = '\n';
    fclose(file);
    print_num("Read length: ", "\nCompiling...", 14, 15, keys_file_len);

    compile(keys_file, keys_file_len);
    if (crash > 1)
    {
        print_num("Error: ", " warnings generated", 8, 20, crash - 1);
        exit(EXIT_FAILURE);
    }
    free(keys_file);
    puts("Done compiling, press Enter to run");
    int c = getchar();
    if (c == '\r' || c == '\n')
        execute();
}