/*
** File   : parse.c
** Author : TK
** Date   :  9/04/94
**
** $Header:   L:/ccpit/vcs/parse.c__   1.4   31 Oct 1994 22:30:24   tk  $
**
** Parse the command line for CCPIT.
*/

/*---------------------------------------------------------------------------
** Includes
*/

#include "ccpit.h"
#include "version.i"

/*---------------------------------------------------------------------------
** Defines and Macros
*/

#define WHAT_OFFSET 4

/*---------------------------------------------------------------------------
** Typedefs
*/

typedef struct {
    char far *cmd_line;
    short cmd_line_len;
    bool in_cfg_file;
} CmdLine;

typedef struct {
    PIT_GROUP *pg;
    int        pit_group;
    int        total_cars;
} ParseState;

/*---------------------------------------------------------------------------
** Local function prototypes
*/

void init_cmd_line(CmdLine *cline, char far *cmd_line, short cmd_line_len);
void init_parse_state(ParseState *ps);
bool is_in_pit_group(ParseState *ps);
void start_pit_group(ParseState *ps);
void ensure_pit_groups(ParseState *ps);
bool do_parse(CmdLine *cline, ParseState *ps);
bool parse_cfg_file(CmdLine *cline, ParseState *ps);
bool parse_option(char c, CmdLine *cline, ParseState *ps);
bool is_parsed_fully(CmdLine *cline);
char parse_char(CmdLine *cline);
short parse_short(CmdLine *cline);

byte reserved_cars_in_seed_group(byte group);
void display_msg(char near *msg);
void display_newline(void);
void display_int(int v);
void display_chr(char c);
void Usage(void);

/*---------------------------------------------------------------------------
** Data
*/

char title_msg[] =
    "@(#)"
    "CCPIT " VERSION " - Grand Prix/World Circuit Computer Cars Pit Strategy.\n"
    "Copyright (c) Trevor Kellaway (CIS:100331,2330) 1994 - All Rights Reserved.\n"
    "Copyright (c) Rene Smit 2020 - All Rights Reserved.\n\n";

/*---------------------------------------------------------------------------
** Functions
*/

bool
parse(
    char far *cmd_line,
    short     cmd_line_len
) {
    byte       index;
    CmdLine    cline;
    ParseState ps;

    init_parse_state(&ps);
    init_cmd_line(&cline, cmd_line, cmd_line_len);

    display_msg(&title_msg[WHAT_OFFSET]);

    tmp_num_groups      = 0;
    tmp_max_cars_in_pit = DEFAULT_MAX_CARS_IN_PIT;
    tmp_randomise       = FALSE;
    tmp_tyres           = 0;
    tmp_multiplayer     = FALSE;

    for (index = 0; index < 40; index++) {
        tmp_reserved_pit_groups[index] = 0;
    }

    if (!do_parse(&cline, &ps)) {
        return FALSE;
    }

    ensure_pit_groups(&ps);

    tmp_num_groups = ps.pit_group;

    return TRUE;
}

void
init_cmd_line(
    CmdLine  *cline,
    char far *cmd_line,
    short     cmd_line_len
) {
    int i;
    bool comment = FALSE;

    cline->cmd_line = cmd_line;
    cline->cmd_line_len = cmd_line_len;
    cline->in_cfg_file = cmd_line == cfg_data;

    for (i = 0; i <= cmd_line_len; i++) {
        int newline = cmd_line[i] == '\r' || cmd_line[i] == '\n';
        if (newline) {
            comment = FALSE;
        }
        else if (cline->in_cfg_file && cmd_line[i] == ';') {
            comment = TRUE;
        }
        if (comment || newline || cmd_line[i] == ' ' || cmd_line[i] == '\t') {
            cmd_line[i] = '\0';
        }
    }
}

void
init_parse_state(
    ParseState *ps
) {
    ps->pg = &tmp_pit_groups[0];
    ps->pit_group = 0;
    ps->total_cars = 0;
}

bool
is_in_pit_group(
    ParseState *ps
) {
    return ps->pit_group != 0;
}

void
start_pit_group(
    ParseState *ps
) {
    if (!is_in_pit_group(ps) ||
            ps->pg->num_cars > 0 ||
            reserved_cars_in_seed_group(ps->pit_group) > 0) {
        
        ps->pg = &tmp_pit_groups[ps->pit_group++];
        ps->pg->num_cars = 0;
        ps->pg->tyres = 0;
    }
}

void
ensure_pit_groups(
    ParseState *ps
) {
    if (!is_in_pit_group(ps)) {
        /*
        ** Nothing defined so use defaults.
        */
        start_pit_group(ps);
        ps->pg->num_cars   = 13;
        ps->pg->num_stops  = 2;
        ps->pg->percent[0] = 25;
        ps->pg->percent[1] = 55;

        start_pit_group(ps);
        ps->pg->num_cars   = 13;
        ps->pg->num_stops  = 2;
        ps->pg->percent[0] = 35;
        ps->pg->percent[1] = 65;
    }
}

bool
do_parse(
    CmdLine *cline,
    ParseState *ps
) {
    char c;

    while (!is_parsed_fully(cline) && !unload_flag) {
        c = parse_char(cline);
        if (c == '@') {
            if (!parse_cfg_file(cline, ps)) {
                return FALSE;
            }
        }
        else if (c == '-' || c == '/') {
            if (is_parsed_fully(cline) ||
                    !parse_option(parse_char(cline), cline, ps)) {
                return FALSE;
            }
        }
        else if (is_in_pit_group(ps) &&
                (c == '#' || c == 'c' || c == 't' || c == '%' || c == 'l')) {
            if (!parse_option(c, cline, ps)) {
                return FALSE;
            }
        }
        else if (c) {
            display_msg("Unexpected argument: ");
            while (c) {
                display_chr(c);
                c = parse_char(cline);
            }
            display_newline();
            return FALSE;
        }
    }

    return TRUE;
}

bool
parse_cfg_file(
    CmdLine *cline,
    ParseState *ps
) {
    char c;
    int  n;
    word cfg_len;
    char *pCfg = cfg_filename;

    if (cline->in_cfg_file) {
        display_msg("@ option inside config file is not allowed\n");
        return FALSE;
    }

    n = 0;
    while (!is_parsed_fully(cline) && n < sizeof(cfg_filename) - 1) {
        c = parse_char(cline);
        *pCfg++ = c;
        ++n;
        if (!c) {
            break;
        }
    }
    cfg_len = read_cfg_file();
    if (cfg_len > 0) {
        CmdLine cfg_cline;
        init_cmd_line(&cfg_cline, cfg_data, cfg_len);
        if (!do_parse(&cfg_cline, ps)) {
            return FALSE;
        }
    } else {
        display_msg("Failed to read config file: ");
        display_msg(cfg_filename);
        display_chr('\n');
        return FALSE;
    }
    return TRUE;
}

bool
parse_option(
    char        c,
    CmdLine    *cline,
    ParseState *ps
) {
    int        n;

    /*
    ** Don't use switch as indirect table gets it wrong for a COM file.
    */
    if (c == 'h' || c == '?') {
        Usage();
        return FALSE;
    }
    else if (c == 'u') {
        unload_flag = TRUE;
    }
    else if (c == 'm') {
        tmp_multiplayer = TRUE;
    }
    else if (c == 'r') {
        tmp_randomise = TRUE;
    }
    else if (c == 'p') {
        n = parse_short(cline);
        if (n >= 5 && n <= 26) {
            tmp_max_cars_in_pit = (byte) n;
        }
        else {
            display_msg("ccpit: -p value should be between 5 and 26.\n");
            return FALSE;
        }
    }
    else if (c == 'g') {
        if (ps->pit_group < MAX_GROUPS) {
            start_pit_group(ps);
        }
        else {
            display_msg("ccpit: Too many -g pit groups defined.\n");
            return FALSE;
        }
    }
    else if (c == 't') {
        char t = parse_char(cline);
        if (t >= 'A' && t <= 'D') {
            if (!is_in_pit_group(ps)) {
                tmp_tyres = t;
            }
            else if (ps->pg->num_stops == 0) {
                init_group_tyre(ps->pg, t - 'A');
            }
            else if (ps->pg->tyres) {
                set_group_pit_tyre(ps->pg, ps->pg->num_stops - 1, t - 'A');
            }
            else {
                display_msg("ccpit: Cannot specify pit stop tyres without specifying race start tyres.\n");
                return FALSE;
            }
        }
        else {
            display_msg("ccpit: Tyres must be one 'A', 'B', 'C', or 'D'.\n");
            return FALSE;
        }
    }
    else if (c == '#') {
        if (!is_in_pit_group(ps)) {
            display_msg("ccpit: You must use -g before a -#.\n");
            return FALSE;
        }
        n = parse_short(cline);
        if (n >= 1 && n <= 40) {
            if (tmp_reserved_pit_groups[n - 1] != 0) {
                display_msg("ccpit: A car cannot be assigned to multiple pit groups.\n");
                return FALSE;
            }
            tmp_reserved_pit_groups[n - 1] = ps->pit_group;
        }
        else {
            display_msg("ccpit: -# value should be between 1 and 40.\n");
            return FALSE;
        }
    }
    else if (c == 'c') {
        if (!is_in_pit_group(ps)) {
            display_msg("ccpit: You must use -g before a -c.\n");
            return FALSE;
        }
        if (ps->pg->num_cars != 0) {
            display_msg("ccpit: -c option may only be specified once per pit group.\n");
            return FALSE;
        }
        n = parse_short(cline);
        if (n >= 0 && n <= 26) {
            ps->total_cars += n;
            if (ps->total_cars > 26) {
                display_msg("ccpit: Total number of cars in pit group can't exceed 26.\n");
                return FALSE;
            }
            ps->pg->num_cars = (byte) n;
        }
        else {
            display_msg("ccpit: -c value should be between 0 and 26.\n");
            return FALSE;
        }
    }
    else if (c == '%' || c == 'l') {
        if (!is_in_pit_group(ps)) {
            display_msg("ccpit: You must use -g before a -%.\n");
            return FALSE;
        }
        if (ps->pg->num_stops < MAX_PITS_PER_GROUP) {
            n = parse_short(cline);
            if (n >= 5 && n <= 95) {
                if (ps->pg->num_stops > 0 && (byte) n <= ps->pg->percent[ps->pg->num_stops - 1]) {
                    display_msg("ccpit: -% percentage values should be in increasing order.\n");
                    return FALSE;
                }
                ps->pg->percent[ps->pg->num_stops++] = (byte) n;
            }
            else {
                display_msg("ccpit: -% percentage value should be between 5 and 95.\n");
                return FALSE;
            }
        }
        else {
            display_msg("ccpit: Too many stops for a group.\n");
            return FALSE;
        }
    }
    else {
        display_msg("Unknown option: ");
        while (c) {
            display_chr(c);
            c = parse_char(cline);
        }
        display_msg("\n\nRun with option -h for help.\n");
        return FALSE;
    }

    return TRUE;
}

bool
is_parsed_fully(
    CmdLine *cline
) {
    return cline->cmd_line_len <= 0;
}

char
parse_char(
    CmdLine *cline
) {
    char c = *cline->cmd_line;
    cline->cmd_line_len--;
    cline->cmd_line++;
    return c;
}

/*lint +e789 Ignore assigning auto to static */

short
parse_short(
    CmdLine *cline
) {
    int  n;
    bool neg;
    char c;

    n = 0;
    neg = FALSE;
    while (!is_parsed_fully(cline)) {
        c = parse_char(cline);
        switch (c) {
        case ' ':
        case '\t':
            continue;
        case '-':
            neg = TRUE;
            /*lint fall through */
        case '+':
            break;
        default:
            break;
        }
        break;
    }
    while (c >= '0' && c <= '9') {
        n = n * 10 + c - '0';
        if (is_parsed_fully(cline)) {
            break;
        }
        c = parse_char(cline);
    }
    return (neg ? -n : n);
}


byte
reserved_cars_in_seed_group(
    byte group
) {
    byte count = 0;
    byte index;

    for (index = 0; index < 40; index++) {
        if (tmp_reserved_pit_groups[index] == group) {
            count++;
        }
    }
    return count;
}


void
display_newline(void) {
    display_msg("\n");
}

void
dump_count(
    int count,
    char near *name,
    char near *plural
) {
    display_int(count);
    display_chr(' ');
    display_msg(count == 1 ? name : plural);
}

void
dump_num_cars(
    int num_cars
) {
    dump_count(num_cars, "car", "cars");
}

void
dump_tyre_compound(
    char tyre_compound
) {
    display_chr(tyre_compound);
    display_msg("'s");
}

void
dump_percentage(
    int percentage
) {
    display_int(percentage);
    display_chr('%');
}

void
dump_group_hdr(
    int num_cars
) {
    display_msg(" -> ");
    dump_num_cars(num_cars);
    display_newline();
}

void
dump_group_item_prefix(void) {
    display_msg("    - ");
}

void
dump_group_item_hdr(
    char near *hdr
) {
    dump_group_item_prefix();
    display_msg(hdr);
}

void
dump_reserved_cars(
    int count,
    int group_index
) {
    int i;
    bool first = TRUE;

    dump_group_item_prefix();
    dump_num_cars(count);
    display_msg(" reserved: ");
    for (i = 0; i < 40; i++) {
        if (tmp_reserved_pit_groups[i] == group_index) {
            if (!first) {
                display_msg(", ");
            }
            display_msg("#");
            display_int(i + 1);
            first = FALSE;
        }
    }
    display_newline();
}

void
dump_tyres_start(
    byte tyres
) {
    dump_group_item_hdr("starting on ");
    dump_tyre_compound(tyres);
    display_newline();
}

void
dump_no_stops(
    void
) {
    dump_group_item_hdr("making no pit stops\n");
}


/*
** Show parsed pit group configuration.
** Called from install.asm when parsing successful and ready to be installed.
*/
void
dump_config(
    void
) {
    register PIT_GROUP *pg;
    int                 i;
    int                 j;
    int                 remaining_cars;
    int                 g;
    int                 r;

    g = 0;
    remaining_cars = 26;
    for (i = 0, pg = tmp_pit_groups; i < tmp_num_groups; i++, pg++) {
        r = reserved_cars_in_seed_group(i + 1);
        if (r > 0 || pg->num_cars > 0) {
            ++g;
            remaining_cars -= r + pg->num_cars;
        }
        else {
            display_msg("Warning: empty pit group.\n");
        }
    }
    if (remaining_cars > 0) {
        ++g;
    }

    display_msg("Maximum ");
    dump_num_cars(tmp_max_cars_in_pit);
    display_msg(" in the pits");
    display_newline();
    if (tmp_randomise) {
        display_msg("Group allocation on grid will be randomised\n");
    }
    if (tmp_multiplayer) {
        display_msg("Local multi-player mode enabled\n");
    }

    display_msg("Using ");
    dump_count(g, "pit group", "pit groups");
    display_msg(":\n");
    for (i = 0, pg = tmp_pit_groups; i < tmp_num_groups; i++, pg++) {
        r = reserved_cars_in_seed_group(i + 1);
        if (r > 0 || pg->num_cars > 0) {
            dump_group_hdr(r + pg->num_cars);
            if (r > 0) {
                dump_reserved_cars(r, i + 1);
            }
            if (pg->tyres != 0 || tmp_tyres != 0) {
                dump_tyres_start(pg->tyres ? pg->tyres : tmp_tyres);
            }
            for (j = 0; j < MAX_PITS_PER_GROUP; j++) {
                if (pg->percent[j] == 0) {
                    if (j == 0) {
                        dump_no_stops();
                    }
                    break;
                }
                dump_group_item_hdr("pitting @ ");
                dump_percentage(pg->percent[j]);
                if (pg->tyres != 0 || tmp_tyres != 0) {
                    display_msg(" for ");
                    if (pg->tyres != 0) {
                        dump_tyre_compound('A' + get_group_pit_tyre(pg, j));
                    }
                    else {
                        dump_tyre_compound(tmp_tyres);
                    }
                }
                display_newline();
            }
        }
    }
    if (remaining_cars > 0) {
        dump_group_hdr(remaining_cars);
        if (tmp_tyres != 0) {
            dump_tyres_start(tmp_tyres);
        }
        dump_no_stops();
    }
    if (tmp_num_groups > 4) {
        display_msg("Warning: drivers of cars 3");
        display_int(10 - tmp_num_groups);
        display_msg(" through 39 will be named #3");
        display_int(10 - tmp_num_groups);
        display_msg(" through #39 resp.\n");
    }
    display_newline();
}

void
display_msg(
    char near *msg                     /* In Msg to display               */
) {
    while (*msg) {
        if (*msg == '\n') {
            display_chr('\r');
        }
        display_chr(*msg++);
    }
}

void
display_int(
    int v
) {
    int d;
    int i;
    int b = 0;
    if (v == 0) {
        display_chr('0');
    }
    else {
        if (v < 0) {
            display_chr('-');
            v = -v;
        }
        for (i = 0, d = 10000; i < 5; i++, d /= 10) {
            if (b || v / d) {
                display_chr('0' + v / d);
                b = 1;
            }
            v = v % d;
        }
    }
}

/*lint -e789 Ignore assigning auto to static */

void
display_chr(
    char c                             /* In Character to display         */
) {
    char cs[2];

    cs[0]    = c;
    cs[1]    = '$';
    msg_text = cs;
    wrt_msg();
}

void
Usage(
    void
) {
    display_msg("Usage: ccpit [@filename] [-h] [-u] [-pN] [-r] [-m] [-t?]\n"
                "             {-g -#N -cN [-t?] (-%N [-t?]) ...} ...\n"
                "\n"
                "       @filename Read options from filename.\n"
                "       -h,-?     This help message.\n"
                "       -u        Unload TSR.\n"
                "\n"
                "       -pN       Max. number cars in pits at one time (default 10).\n"
                "       -r        Randomise group allocation on grid (default grid order).\n"
                "\n"
                "       -m        Enable local multi-player mode (player's car called to pit).\n"
                "       -t?       Specify tyres for all computer cars, where ? is one of ABCD.\n"
                "\n"
                "       -g        Pit group.\n"
                "        -#N      Stop car number N (for this group)\n"
                "        -cN      Stop another N cars (for this group).\n"
                "        -t?      Race start tyres (for this group), where ? is one of ABCD.\n"
                "        -%N      Trigger cars to stop at race percentage N (for this group).\n"
                "         -t?     Tyres for this pit stop, where ? is one of ABCD.\n"
               );
}

/*---------------------------------------------------------------------------
** End of File
*/


