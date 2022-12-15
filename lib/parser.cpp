#include "parser.h"

#include <algorithm>
#include <cassert>
#include <fstream>
#include <stack>
#include <stdexcept>

std::vector<std::string> ParseWay(const std::string& str);
bool IsDigit(char c);
bool CheckKeyValidity(std::string_view key);
omfl::Types GetValueType(std::string_view value);
std::pair<std::any, bool> ConvertValue(const std::string& value, const omfl::Types& type);
void PrettifyString(std::string& str);
std::pair<std::vector<std::string>, bool> ParseSections(std::string_view str, size_t& index);

omfl::Item::Item()
    : value_type(Types::Undefined)
{};

omfl::Item::Item(std::string_view _key, const std::any& _value, const Types& _value_type)
    : key(_key)
    , value(_value)
    , value_type(_value_type)
{}

std::vector<std::string> ParseWay(const std::string& str) {
    std::vector<std::string> result;
    std::string buff;

    for (auto c: str) {
        if (c == '.') {
            result.emplace_back(buff);
            buff.clear();
        } else {
            buff.push_back(c);
        }
    }

    if (!buff.empty()) {
        result.emplace_back(buff);
    }

    return result;
}

const omfl::Item& omfl::Item::Get(const std::string& name) const {
    if (name.find('.') != std::string::npos) {
        return Get(ParseWay(name), 0);
    }

    if (value_type != Types::Section) {
        return *this;
    }

    const std::map<std::string, Item>& items = std::any_cast<const std::map<std::string, Item>&>(value);

    if (items.find(name) == items.end()) {
        throw std::runtime_error("Addressing to an non-existing key/section.");
    }

    return items.at(name);
}

const omfl::Item& omfl::Item::Get(const std::vector<std::string>& way, size_t current) const {
    if (current == way.size()) {
        return *this;
    }

    const std::map<std::string, Item>& items = std::any_cast<const std::map<std::string, Item>&>(value);

    return items.at(way[current]).Get(way, current + 1);
}

bool omfl::Item::IsInt() const {
    return value_type == Types::Integer;
}

int32_t omfl::Item::AsInt() const {
    return std::any_cast<int32_t>(value);
}

int32_t omfl::Item::AsIntOrDefault(int32_t value) const {
    if (IsInt()) {
        return AsInt();
    }

    return value;
}

bool omfl::Item::IsFloat() const {
    return value_type == Types::Float;
}

double omfl::Item::AsFloat() const {
    return std::any_cast<double>(value);
}

double omfl::Item::AsFloatOrDefault(double value) const {
    if (IsFloat()) {
        return AsFloat();
    }

    return value;
}

bool omfl::Item::IsString() const {
    return value_type == Types::String;
}

std::string_view omfl::Item::AsString() const {
    return std::any_cast<const std::string&>(value);
}

std::string_view omfl::Item::AsStringOrDefault(std::string_view value) const {
    if (IsString()) {
        return AsString();
    }

    return value;
}

bool omfl::Item::IsBool() const {
    return value_type == Types::Boolean;
}

bool omfl::Item::AsBool() const {
    return std::any_cast<bool>(value);
}

bool omfl::Item::AsBoolOrDefault(bool value) const {
    if (IsBool()) {
        return AsBool();
    }

    return value;
}

bool omfl::Item::IsArray() const {
    return value_type == Types::Array;
}

const omfl::Item& omfl::Item::operator[](size_t index) const {
    if (!IsArray()) {
        throw std::runtime_error("Trying to access non-accessible value.");
    }

    const ValueArray& array = std::any_cast<const ValueArray&>(value);

    return array.Get(index);
}

omfl::ValueArray::ValueArray()
    : trash_item_(Item("", "", Types::Undefined))
{}

void omfl::ValueArray::Add(const std::any& value, const Types& type) {
    values_.push_back(Item("", value, type));
}

const omfl::Item& omfl::ValueArray::Get(size_t index) const {
    if (index >= values_.size()) {
        return trash_item_;
    }

    return values_[index];
}

omfl::Parser::Parser()
    : successful_parse_(true)
{}

bool omfl::Parser::valid() const {
    return successful_parse_;
}

void omfl::Parser::MarkUnsuccessful() {
    successful_parse_ = false;
}

bool omfl::Parser::Add(const std::vector<std::string>& section_way, const Item& appending_item) {
    return tree_.AddItem(section_way, appending_item);
}

const omfl::Item& omfl::Parser::Get(const std::string& name) const {
    return tree_.GetItem(name);
}

omfl::Parser::Trie::Trie() {
    root_ = Item("", std::map<std::string, Item>(), Types::Section);
}

bool omfl::Parser::Trie::AddItem(const std::vector<std::string>& section_way, const Item& appending_item) {
    Item* current_node = &root_;
    
    for (const auto& section: section_way) {
        std::map<std::string, Item>& items = std::any_cast<std::map<std::string, Item>&>(current_node->value);

        if (items.find(section) == items.end()) {
            items[section] = Item(section, std::map<std::string, Item>(), Types::Section);
        }

        current_node = &items[section];
    }

    std::map<std::string, Item>& items = std::any_cast<std::map<std::string, Item>&>(current_node->value);

    if (items.find(appending_item.key) != items.end()) {
        return false;
    }

    items[appending_item.key] = appending_item;

    return true;
}

const omfl::Item& omfl::Parser::Trie::GetItem(const std::string& name) const {
    return root_.Get(name);
}

bool IsDigit(char c) {
    return '0' <= c && c <= '9';
}

bool CheckKeyValidity(std::string_view key) {
    if (key.empty()) {
        return false;
    }

    for (auto character: key) {
        if (
            !('a' <= character && character <= 'z') &&
            !('A' <= character && character <= 'Z') &&
            !IsDigit(character) &&
            !(character == '-' || character == '_')
        ) {
            return false;
        }
    }

    return true;
}

omfl::Types GetValueType(std::string_view value) {
    using omfl::Types;
    
    if (value.empty()) {
        return Types::Undefined;
    }

    if (value[0] == '\"' && value.back() == '\"') {
        // Presumably, it is a string.

        if (std::count(value.begin(), value.end(), '\"') == 2) {
            return Types::String;
        } else {
            return Types::Undefined;
        }
    } else if (value[0] == '[' && value.back() == ']') {
        int32_t balance = 0;

        for (auto character: value) {
            if (character == '[') {
                ++balance;
            } else if (character == ']') {
                if (balance == 0) {
                    return Types::Undefined;
                }

                --balance;
            }
        }

        if (balance > 0) {
            return Types::Undefined;
        }

        return Types::Array;
    } else if (value == "true" || value == "false") {
        return Types::Boolean;
    } else {
        if (value[0] == '.') {
            return Types::Undefined;
        }

        if ((value[0] == '+' || value[0] == '-') && value.size() == 1) {
            return Types::Undefined;
        }

        if (
            !IsDigit(value[0]) &&
            !(value[0] == '+' || value[0] == '-')
        ) {
            return Types::Undefined;
        }

        size_t pluses = 0;
        size_t minuses = 0;
        size_t points = 0;
        size_t point_index = value.size() + 1;

        for (size_t i = 0; i < value.size(); ++i) {
            char character = value[i];

            if (character == '+') {
                ++pluses;
            } else if (character == '-') {
                ++minuses;
            } else if (character == '.') {
                ++points;
                point_index = i;
            } else if (!IsDigit(character)){
                return Types::Undefined;
            }
        }

        if (pluses + minuses > 1 || points > 1) {
            return Types::Undefined;
        }

        if ((pluses == 1 || minuses == 1) && value[0] != '+' && value[0] != '-') {
            return Types::Undefined;
        }

        if (points > 0) {
            assert(point_index != value.size() + 1);
            
            if (
                (point_index == 1 && !IsDigit(value[0])) ||
                point_index == value.size() - 1
            ) {
                return Types::Undefined;
            }

            return Types::Float;
        }

        return Types::Integer;
    }

    return Types::Undefined;
}

std::pair<std::any, bool> omfl::ConstructValueArray(const std::string& value) {
    using omfl::Types;

    omfl::ValueArray result;
    std::string buff;
    bool ok = true;
    int32_t balance = 0;

    for (size_t i = 1; i < value.size(); ++i) {
        if ((value[i] == ',' && balance == 0) || i == value.size() - 1) {
            if (buff.empty()) {
                continue;
            }

            PrettifyString(buff);

            Types type = GetValueType(buff);

            if (type == Types::Undefined) {
                ok = false;

                break;
            }

            auto [value, successful] = ConvertValue(buff, type);

            if (!successful) {
                ok = false;

                break;
            }

            result.Add(value, type);
            buff.clear();
        } else {
            buff.push_back(value[i]);

            if (value[i] == '[') {
                ++balance;
            } else if (value[i] == ']') {
                if (balance == 0) {
                    ok = false;

                    break;
                }

                --balance;
            }
        }
    }

    return {result, ok};
}

std::pair<std::any, bool> ConvertValue(const std::string& value, const omfl::Types& type) {
    using omfl::Types;

    if (type == Types::Integer) {
        return {std::any(std::stoi(value)), true};
    } else if (type == Types::Float) {
        return {std::any(std::stod(value)), true};
    } else if (type == Types::String) {
        return {std::any(value.substr(1, value.size() - 2)), true};
    } else if (type == Types::Boolean) {
        if (value == "true") {
            return {std::any(true), true};
        }

        return {std::any(false), true};
    }

    assert(type == Types::Array);

    return omfl::ConstructValueArray(value);
}

void PrettifyString(std::string& str) {
    while (!str.empty() && str.back() == ' ') {
        str.pop_back();
    }

    size_t prefix_spaces = 0;

    for (; prefix_spaces < str.size(); ++prefix_spaces) {
        if (str[prefix_spaces] != ' ') {
            break;
        }
    }

    str = str.substr(prefix_spaces);
}

bool omfl::Update(omfl::Parser& parser, const std::vector<std::string>& current_sections, std::string& current_key, std::string& current_value) {
    PrettifyString(current_key);
    PrettifyString(current_value);
    
    if (current_key.empty() && current_value.empty()) {
        return true;
    }

    if (!CheckKeyValidity(current_key)) {
        return false;
    }

    Types value_type = GetValueType(current_value);

    if (value_type == Types::Undefined) {
        return false;
    }

    auto [converted_value, successful] = ConvertValue(current_value, value_type);

    if (!successful) {
        return false;
    }

    bool ok = parser.Add(current_sections, Item(current_key, converted_value, value_type));

    if (!ok) {
        return false;
    }

    current_key.clear();
    current_value.clear();

    return true;
}

std::pair<std::vector<std::string>, bool> ParseSections(std::string_view str, size_t& index) {
    std::vector<std::string> result;
    std::string buff;
    bool ok = true;

    for (; index < str.size() && str[index] != '\n'; ++index) {
        if (str[index] == '.' || str[index] == ']') {
            ok &= CheckKeyValidity(buff);
            result.emplace_back(buff);
            buff.clear();
        } else {
            buff += str[index];
        }
    }

    assert(buff.empty());

    return {result, ok};
}

std::pair<std::vector<std::string>, bool> ParseSections(std::ifstream& stream, size_t& index) {
    std::vector<std::string> result;
    std::string buff;
    bool ok = true;
    char character = '#';

    for (; stream.get(character) && character != '\n'; ++index) {
        if (character == '.' || character == ']') {
            ok &= CheckKeyValidity(buff);
            result.emplace_back(buff);
            buff.clear();
        } else {
            buff += character;
        }
    }

    assert(buff.empty());

    return {result, ok};
}

omfl::Parser omfl::parse(const std::filesystem::path& path) {
    Parser parser;
    std::ifstream stream(path);

    if (!stream.is_open()) {
        throw std::runtime_error("No such file as " + path.filename().string());
    }

    std::vector<std::string> current_sections;
    std::string current_key;
    std::string current_value;
    bool equal_sign_seen = false;
    bool in_string = false;
    bool ignore = false;
    char character;

    for (size_t index = 0; stream.get(character); ++index) {
        if (character == '[' && !equal_sign_seen) {
            auto [current_sections_, successful] = ParseSections(stream, ++index);
            current_sections = current_sections_;

            if (!successful) {
                parser.MarkUnsuccessful();

                break;
            }

            current_key.clear();
            current_value.clear();

            continue;
        }
        
        if (character == '\n') {
            equal_sign_seen = false;
            in_string = false;
            ignore = false;

            if (!Update(parser, current_sections, current_key, current_value)) {
                parser.MarkUnsuccessful();

                break;
            }

            continue;
        }

        if (character == '#' && !in_string) {
            ignore = true;
        }

        if (ignore) {
            continue;
        }

        if (character == '=' && !in_string) {
            if (equal_sign_seen) {
                parser.MarkUnsuccessful();
                
                break;
            }

            equal_sign_seen = true;

            continue;
        }

        if (!equal_sign_seen) {
            current_key.push_back(character);
        } else {
            if (character == '\"') {
                in_string ^= 1;
            }

            current_value.push_back(character);
        }
    }

    if ((!current_key.empty() || !current_value.empty()) && parser.valid()) {
        if (!Update(parser, current_sections, current_key, current_value)) {
            parser.MarkUnsuccessful();
        }
    }

    return parser;
}

omfl::Parser omfl::parse(const std::string& str) {
    Parser parser;

    std::vector<std::string> current_sections;
    std::string current_key;
    std::string current_value;
    bool equal_sign_seen = false;
    bool in_string = false;
    bool ignore = false;

    for (size_t index = 0; index < str.size(); ++index) {
        char character = str[index];

        if (character == '[' && !equal_sign_seen) {
            auto [current_sections_, successful] = ParseSections(str, ++index);
            current_sections = current_sections_;

            if (!successful) {
                parser.MarkUnsuccessful();

                break;
            }

            current_key.clear();
            current_value.clear();

            continue;
        }
        
        if (character == '\n') {
            equal_sign_seen = false;
            in_string = false;
            ignore = false;

            if (!Update(parser, current_sections, current_key, current_value)) {
                parser.MarkUnsuccessful();

                break;
            }

            continue;
        }

        if (character == '#' && !in_string) {
            ignore = true;
        }

        if (ignore) {
            continue;
        }

        if (character == '=' && !in_string) {
            if (equal_sign_seen) {
                parser.MarkUnsuccessful();
                
                break;
            }

            equal_sign_seen = true;

            continue;
        }

        if (!equal_sign_seen) {
            current_key.push_back(character);
        } else {
            if (character == '\"') {
                in_string ^= 1;
            }

            current_value.push_back(character);
        }
    }

    if ((!current_key.empty() || !current_value.empty()) && parser.valid()) {
        if (!Update(parser, current_sections, current_key, current_value)) {
            parser.MarkUnsuccessful();
        }
    }

    return parser;
}
