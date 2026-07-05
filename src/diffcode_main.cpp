#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

static const char *PATCHCODE_SEPARATOR = "<@+@+@>";

struct LineFile {
    vector<string> lines;
    bool had_nul;
};

struct Hunk {
    size_t old_start;
    size_t old_end;
    size_t new_start;
    size_t new_end;
};

struct Options {
    string old_file;
    string new_file;
    string out_file;
    string target_filename;
    string author;
    string patchname;
    string bugs;
    string extra;
    bool enable;
    size_t context;
    size_t max_context;
};

static bool read_physical_line(istream &in, string &line) {
    line.clear();
    char c = 0;
    while (in.get(c)) {
        line.push_back(c);
        if (c == '\n')
            break;
    }
    return !line.empty();
}

static LineFile read_lines(const string &path, bool &ok) {
    LineFile result;
    result.had_nul = false;
    ok = false;

    fstream f(path.c_str(), ios::in | ios::binary);
    if (!f.is_open()) {
        cerr << "Error: unable to open file: " << path << endl;
        return result;
    }

    string line;
    while (read_physical_line(f, line)) {
        if (line.find('\0') != string::npos)
            result.had_nul = true;
        result.lines.push_back(line);
    }
    ok = true;
    return result;
}

static bool has_eol(const string &line) {
    return !line.empty() && (line[line.size() - 1] == '\n' || line[line.size() - 1] == '\r');
}

static void write_payload_line(ostream &out, const string &line) {
    out << line;
    if (!has_eol(line))
        out << '\n';
}

static string timestamp_now() {
    time_t t = time(0);
    struct tm tm_value;
#if defined(_WIN32)
    localtime_s(&tm_value, &t);
#else
    localtime_r(&t, &tm_value);
#endif
    char buf[64];
    strftime(buf, sizeof(buf), "%Y/%m/%d-%H:%M:%S", &tm_value);
    return string(buf);
}

static size_t sequence_count(const vector<string> &lines, size_t start, size_t end) {
    if (start >= end || end > lines.size())
        return 0;
    const size_t len = end - start;
    size_t count = 0;
    for (size_t pos = 0; pos + len <= lines.size(); ++pos) {
        bool same = true;
        for (size_t i = 0; i < len; ++i) {
            if (lines[pos + i] != lines[start + i]) {
                same = false;
                break;
            }
        }
        if (same)
            ++count;
    }
    return count;
}

static void choose_anchor_before(const vector<string> &old_lines,
                                 size_t hunk_start,
                                 size_t min_context,
                                 size_t max_context,
                                 size_t &anchor_start,
                                 size_t &anchor_end) {
    anchor_start = hunk_start;
    anchor_end = hunk_start;
    if (hunk_start == 0)
        return;

    size_t len = min_context;
    if (len == 0)
        len = 1;
    if (len > hunk_start)
        len = hunk_start;

    const size_t max_len = std::min(hunk_start, std::max(len, max_context));
    while (true) {
        anchor_start = hunk_start - len;
        anchor_end = hunk_start;
        if (sequence_count(old_lines, anchor_start, anchor_end) <= 1 || len >= max_len)
            break;
        ++len;
    }
}

static void choose_anchor_after(const vector<string> &old_lines,
                                size_t hunk_end,
                                size_t min_context,
                                size_t max_context,
                                size_t &anchor_start,
                                size_t &anchor_end) {
    anchor_start = hunk_end;
    anchor_end = hunk_end;
    const size_t available = old_lines.size() - hunk_end;
    if (available == 0)
        return;

    size_t len = min_context;
    if (len == 0)
        len = 1;
    if (len > available)
        len = available;

    const size_t max_len = std::min(available, std::max(len, max_context));
    while (true) {
        anchor_start = hunk_end;
        anchor_end = hunk_end + len;
        if (sequence_count(old_lines, anchor_start, anchor_end) <= 1 || len >= max_len)
            break;
        ++len;
    }
}

static vector<Hunk> fallback_hunks(const vector<string> &old_lines, const vector<string> &new_lines) {
    vector<Hunk> hunks;
    size_t prefix = 0;
    while (prefix < old_lines.size() && prefix < new_lines.size() && old_lines[prefix] == new_lines[prefix])
        ++prefix;

    size_t old_suffix = old_lines.size();
    size_t new_suffix = new_lines.size();
    while (old_suffix > prefix && new_suffix > prefix && old_lines[old_suffix - 1] == new_lines[new_suffix - 1]) {
        --old_suffix;
        --new_suffix;
    }

    if (prefix != old_suffix || prefix != new_suffix) {
        Hunk h;
        h.old_start = prefix;
        h.old_end = old_suffix;
        h.new_start = prefix;
        h.new_end = new_suffix;
        hunks.push_back(h);
    }
    return hunks;
}

static vector<Hunk> lcs_hunks(const vector<string> &old_lines, const vector<string> &new_lines, size_t merge_gap) {
    const size_t n = old_lines.size();
    const size_t m = new_lines.size();

    if (n == 0 && m == 0)
        return vector<Hunk>();

    const size_t cells = (n + 1) * (m + 1);
    if (n > 0 && m > 0 && cells / (n + 1) != (m + 1))
        return fallback_hunks(old_lines, new_lines);
    if (cells > 25000000UL) {
        cerr << "Warning: files are large; using prefix/suffix diff fallback instead of full LCS" << endl;
        return fallback_hunks(old_lines, new_lines);
    }

    vector<unsigned int> dp(cells, 0);
    for (size_t i = 1; i <= n; ++i) {
        for (size_t j = 1; j <= m; ++j) {
            if (old_lines[i - 1] == new_lines[j - 1])
                dp[i * (m + 1) + j] = dp[(i - 1) * (m + 1) + (j - 1)] + 1;
            else
                dp[i * (m + 1) + j] = std::max(dp[(i - 1) * (m + 1) + j], dp[i * (m + 1) + (j - 1)]);
        }
    }

    struct Op { char type; };
    vector<Op> ops;
    size_t i = n;
    size_t j = m;
    while (i > 0 || j > 0) {
        if (i > 0 && j > 0 && old_lines[i - 1] == new_lines[j - 1]) {
            Op op; op.type = '='; ops.push_back(op); --i; --j;
        } else if (i > 0 && (j == 0 || dp[(i - 1) * (m + 1) + j] >= dp[i * (m + 1) + (j - 1)])) {
            Op op; op.type = '-'; ops.push_back(op); --i;
        } else {
            Op op; op.type = '+'; ops.push_back(op); --j;
        }
    }
    reverse(ops.begin(), ops.end());

    vector<Hunk> hunks;
    size_t old_pos = 0;
    size_t new_pos = 0;
    bool in_hunk = false;
    Hunk cur;

    for (size_t k = 0; k < ops.size(); ++k) {
        const char t = ops[k].type;
        if (t == '=') {
            if (in_hunk) {
                cur.old_end = old_pos;
                cur.new_end = new_pos;
                hunks.push_back(cur);
                in_hunk = false;
            }
            ++old_pos;
            ++new_pos;
        } else {
            if (!in_hunk) {
                in_hunk = true;
                cur.old_start = old_pos;
                cur.new_start = new_pos;
                cur.old_end = old_pos;
                cur.new_end = new_pos;
            }
            if (t == '-')
                ++old_pos;
            else
                ++new_pos;
        }
    }
    if (in_hunk) {
        cur.old_end = old_pos;
        cur.new_end = new_pos;
        hunks.push_back(cur);
    }

    if (hunks.empty())
        return hunks;

    vector<Hunk> merged;
    merged.push_back(hunks[0]);
    for (size_t h = 1; h < hunks.size(); ++h) {
        Hunk &last = merged.back();
        const bool close_in_old = hunks[h].old_start <= last.old_end + merge_gap;
        const bool close_in_new = hunks[h].new_start <= last.new_end + merge_gap;
        if (close_in_old || close_in_new) {
            last.old_end = hunks[h].old_end;
            last.new_end = hunks[h].new_end;
        } else {
            merged.push_back(hunks[h]);
        }
    }
    return merged;
}

static void show_help() {
    cout << "diffcode command help" << endl << endl;
    cout << "diffcode [options] <original-file> <updated-file> [output.patch]" << endl << endl;
    cout << "Generates a patchcode patch file using the current " << PATCHCODE_SEPARATOR << " section separator." << endl;
    cout << "The generated patch contains metadata headers plus start, updated-code, and end pattern sections." << endl << endl;
    cout << "Options:" << endl;
    cout << "  -h, --help                 Show help" << endl;
    cout << "  -o <file>                  Write patch output to file instead of stdout" << endl;
    cout << "  --filename <path>          filename: value written into patch metadata" << endl;
    cout << "  --author <text>            author: value; default is <fill-author>" << endl;
    cout << "  --patchname <text>         patchname: prefix; hunk number is appended when needed" << endl;
    cout << "  --bugs <text>              bugs: value; default is <fill-bug-ids>" << endl;
    cout << "  --extra <text>             extra: value" << endl;
    cout << "  --context <n>              Minimum anchor context lines; default 3" << endl;
    cout << "  --max-context <n>          Expand anchors up to this many lines to reduce duplicates; default 8" << endl;
    cout << "  --disable                  Emit enable:no instead of enable:yes" << endl;
}

static bool parse_size(const string &s, size_t &value) {
    if (s.empty())
        return false;
    char *end = 0;
    unsigned long v = strtoul(s.c_str(), &end, 10);
    if (end == 0 || *end != '\0')
        return false;
    value = static_cast<size_t>(v);
    return true;
}

static bool parse_args(int argc, char **argv, Options &opt) {
    opt.author = "<fill-author>";
    opt.patchname = "<fill-patch-name>";
    opt.bugs = "<fill-bug-ids>";
    opt.extra = "Generated by diffcode; review pattern anchors before applying.";
    opt.enable = true;
    opt.context = 3;
    opt.max_context = 8;

    vector<string> positional;
    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            show_help();
            exit(0);
        } else if (arg == "-o") {
            if (i + 1 >= argc) { cerr << "Error: -o requires a file" << endl; return false; }
            opt.out_file = argv[++i];
        } else if (arg == "--filename") {
            if (i + 1 >= argc) { cerr << "Error: --filename requires a value" << endl; return false; }
            opt.target_filename = argv[++i];
        } else if (arg == "--author") {
            if (i + 1 >= argc) { cerr << "Error: --author requires a value" << endl; return false; }
            opt.author = argv[++i];
        } else if (arg == "--patchname") {
            if (i + 1 >= argc) { cerr << "Error: --patchname requires a value" << endl; return false; }
            opt.patchname = argv[++i];
        } else if (arg == "--bugs") {
            if (i + 1 >= argc) { cerr << "Error: --bugs requires a value" << endl; return false; }
            opt.bugs = argv[++i];
        } else if (arg == "--extra") {
            if (i + 1 >= argc) { cerr << "Error: --extra requires a value" << endl; return false; }
            opt.extra = argv[++i];
        } else if (arg == "--context") {
            if (i + 1 >= argc || !parse_size(argv[i + 1], opt.context)) { cerr << "Error: --context requires a number" << endl; return false; }
            ++i;
        } else if (arg == "--max-context") {
            if (i + 1 >= argc || !parse_size(argv[i + 1], opt.max_context)) { cerr << "Error: --max-context requires a number" << endl; return false; }
            ++i;
        } else if (arg == "--disable") {
            opt.enable = false;
        } else if (!arg.empty() && arg[0] == '-') {
            cerr << "Error: unknown option: " << arg << endl;
            return false;
        } else {
            positional.push_back(arg);
        }
    }

    if (positional.size() < 2 || positional.size() > 3) {
        cerr << "Error: expected <original-file> <updated-file> [output.patch]" << endl;
        return false;
    }
    opt.old_file = positional[0];
    opt.new_file = positional[1];
    if (positional.size() == 3) {
        if (!opt.out_file.empty()) {
            cerr << "Error: output patch supplied twice" << endl;
            return false;
        }
        opt.out_file = positional[2];
    }
    if (opt.target_filename.empty())
        opt.target_filename = opt.old_file;
    if (opt.max_context < opt.context)
        opt.max_context = opt.context;
    return true;
}

static void emit_segment(ostream &out,
                         const Options &opt,
                         const vector<string> &old_lines,
                         const vector<string> &new_lines,
                         const Hunk &hunk,
                         size_t hunk_number,
                         size_t total_hunks) {
    size_t start_a = hunk.old_start;
    size_t start_b = hunk.old_start;
    size_t end_a = hunk.old_end;
    size_t end_b = hunk.old_end;

    choose_anchor_before(old_lines, hunk.old_start, opt.context, opt.max_context, start_a, start_b);
    choose_anchor_after(old_lines, hunk.old_end, opt.context, opt.max_context, end_a, end_b);

    // If both anchors are empty, use the patchcode full-file replacement marker.
    const bool full_file_mode = (start_a == start_b && end_a == end_b);

    out << "# Generated by diffcode. Review the metadata and pattern anchors before applying.\n";
    out << "# Original file: " << opt.old_file << "\n";
    out << "# Updated file: " << opt.new_file << "\n";
    out << "filename:" << opt.target_filename << "\n";
    out << "author:" << opt.author << "\n";
    out << "patchname:" << opt.patchname;
    if (total_hunks > 1)
        out << " hunk " << hunk_number << " of " << total_hunks;
    out << "\n";
    out << "timestamp:" << timestamp_now() << "\n";
    out << "enable:" << (opt.enable ? "yes" : "no") << "\n";
    out << "bugs:" << opt.bugs << "\n";
    out << "extra:" << opt.extra << "\n";
    out << PATCHCODE_SEPARATOR << "\n";

    if (!full_file_mode) {
        for (size_t i = start_a; i < start_b; ++i)
            write_payload_line(out, old_lines[i]);
    }

    out << PATCHCODE_SEPARATOR << "\n";

    for (size_t i = hunk.new_start; i < hunk.new_end; ++i)
        write_payload_line(out, new_lines[i]);

    out << PATCHCODE_SEPARATOR << "\n";

    if (full_file_mode) {
        out << "----++++endoffile++++----\n";
    } else {
        for (size_t i = end_a; i < end_b; ++i)
            write_payload_line(out, old_lines[i]);
    }

    out << PATCHCODE_SEPARATOR << "\n\n";
}

int main(int argc, char **argv) {
    Options opt;
    if (!parse_args(argc, argv, opt)) {
        show_help();
        return 1;
    }

    bool ok_old = false;
    bool ok_new = false;
    LineFile old_file = read_lines(opt.old_file, ok_old);
    LineFile new_file = read_lines(opt.new_file, ok_new);
    if (!ok_old || !ok_new)
        return 2;
    if (old_file.had_nul || new_file.had_nul) {
        cerr << "Error: diffcode only supports text files; NUL byte found" << endl;
        return 3;
    }

    if (old_file.lines == new_file.lines) {
        cerr << "Info: files are identical; no patchcode segments generated" << endl;
        if (!opt.out_file.empty()) {
            fstream out(opt.out_file.c_str(), ios::out | ios::trunc | ios::binary);
            if (!out.is_open()) {
                cerr << "Error: unable to open output patch: " << opt.out_file << endl;
                return 4;
            }
            out << "# Generated by diffcode: files are identical; no patch segments were needed.\n";
        }
        return 0;
    }

    const size_t merge_gap = opt.context * 2;
    vector<Hunk> hunks = lcs_hunks(old_file.lines, new_file.lines, merge_gap);
    if (hunks.empty()) {
        cerr << "Info: no changed hunks detected" << endl;
        return 0;
    }

    ostream *out_stream = &cout;
    fstream out_file;
    if (!opt.out_file.empty()) {
        out_file.open(opt.out_file.c_str(), ios::out | ios::trunc | ios::binary);
        if (!out_file.is_open()) {
            cerr << "Error: unable to open output patch: " << opt.out_file << endl;
            return 4;
        }
        out_stream = &out_file;
    }

    *out_stream << "# patchcode patch file generated by diffcode\n";
    *out_stream << "# Separator: " << PATCHCODE_SEPARATOR << "\n";
    *out_stream << "# Fill or edit author, patchname, bugs, and extra values as needed.\n\n";

    for (size_t i = 0; i < hunks.size(); ++i)
        emit_segment(*out_stream, opt, old_file.lines, new_file.lines, hunks[i], i + 1, hunks.size());

    cerr << "Info: generated " << hunks.size() << " patchcode segment(s)" << endl;
    return 0;
}
