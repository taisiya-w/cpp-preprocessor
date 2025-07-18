#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
using filesystem::path;
using namespace filesystem;

path operator""_p(const char* data, std::size_t sz) {
    return path(data, data + sz);
}

bool Process(istream& input, ostream& output, const path& current_file, const vector<path>& include_directories, size_t& line_number) {
    static regex local_reg(R"/(\s*#\s*include\s*"([^"]*)"\s*)/");
    static regex system_reg(R"/(\s*#\s*include\s*<([^>]*)>\s*)/");

    string line;
    path current_directory = current_file.parent_path();

    while (getline(input, line)) {
        ++line_number;
        smatch m;

        path include_path;
        vector<path> search_paths;
        bool is_included = false;

        if (regex_match(line, m, local_reg)) {
            include_path = string(m[1]);
            search_paths.push_back(current_directory);
            search_paths.insert(search_paths.end(), include_directories.begin(), include_directories.end());
            is_included = true;
        }

        if (regex_match(line, m, system_reg)) {
            include_path = string(m[1]);
            search_paths = include_directories;
            is_included = true;
        }

        if (is_included) {
            bool file_found = false;
            for (const path& new_path : search_paths) {
                path full_path = new_path / include_path;
                ifstream include_file(full_path);
                if (include_file.is_open()) {
                    size_t current_number = 0;
                    if (!Process(include_file, output, full_path, include_directories, current_number)) {
                        return false;
                    }
                    file_found = true;
                    break;
                }
            }

            if (!file_found) {
                cout << "unknown include file " << include_path.string() << " at file " << current_file.string() << " at line " << line_number << endl;
                return false;
            }

        } else {
            output << line << "\n";
        }
    }

    return true;
}

bool Preprocess(const path& in_file, const path& out_file, const vector<path>& include_directories) {
    ifstream input(in_file);
    if (!input.is_open()) {
        return false;
    }
    ofstream output(out_file);
    size_t line_number = 0;
    return Process(input, output, in_file, include_directories, line_number);
}

string GetFileContents(string file) {
    ifstream stream(file);

    // конструируем string по двум итераторам
    return {(istreambuf_iterator<char>(stream)), istreambuf_iterator<char>()};
}

void Test() {
    error_code err;
    filesystem::remove_all("sources"_p, err);
    filesystem::create_directories("sources"_p / "include2"_p / "lib"_p, err);
    filesystem::create_directories("sources"_p / "include1"_p, err);
    filesystem::create_directories("sources"_p / "dir1"_p / "subdir"_p, err);

    {
        ofstream file("sources/a.cpp");
        file << "// this comment before include\n"
                "#include \"dir1/b.h\"\n"
                "// text between b.h and c.h\n"
                "#include \"dir1/d.h\"\n"
                "\n"
                "int SayHello() {\n"
                "    cout << \"hello, world!\" << endl;\n"
                "#   include<dummy.txt>\n"
                "}\n"s;
    }
    {
        ofstream file("sources/dir1/b.h");
        file << "// text from b.h before include\n"
                "#include \"subdir/c.h\"\n"
                "// text from b.h after include"s;
    }
    {
        ofstream file("sources/dir1/subdir/c.h");
        file << "// text from c.h before include\n"
                "#include <std1.h>\n"
                "// text from c.h after include\n"s;
    }
    {
        ofstream file("sources/dir1/d.h");
        file << "// text from d.h before include\n"
                "#include \"lib/std2.h\"\n"
                "// text from d.h after include\n"s;
    }
    {
        ofstream file("sources/include1/std1.h");
        file << "// std1\n"s;
    }
    {
        ofstream file("sources/include2/lib/std2.h");
        file << "// std2\n"s;
    }

    assert((!Preprocess("sources"_p / "a.cpp"_p, "sources"_p / "a.in"_p,
                                  {"sources"_p / "include1"_p,"sources"_p / "include2"_p})));

    ostringstream test_out;
    test_out << "// this comment before include\n"
                "// text from b.h before include\n"
                "// text from c.h before include\n"
                "// std1\n"
                "// text from c.h after include\n"
                "// text from b.h after include\n"
                "// text between b.h and c.h\n"
                "// text from d.h before include\n"
                "// std2\n"
                "// text from d.h after include\n"
                "\n"
                "int SayHello() {\n"
                "    cout << \"hello, world!\" << endl;\n"s;

    assert(GetFileContents("sources/a.in"s) == test_out.str());
}

int main() {
    Test();
}