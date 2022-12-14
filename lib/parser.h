#pragma once

#include <any>
#include <cinttypes>
#include <filesystem>
#include <istream>
#include <map>
#include <string_view>
#include <utility>
#include <vector>

namespace omfl {
    enum Types {
        Undefined,
        Integer,
        Float,
        String,
        Boolean,
        Array
    };

    struct Item {
        std::string key;
        std::any value;
        Types value_type;

        Item();

        explicit Item(std::string_view _key, const std::any& _value, const Types& _value_type);
    };

    class ValueArray {
    public:
        ValueArray();

        void Add(const std::any& value, const Types& type);
    private:
        std::vector<std::any> values_;
        std::vector<Types> types_;
    };

    class Parser {
    public:
        Parser();

        bool valid() const;
        void MarkUnsuccessful();

        void Add(const std::vector<std::string>& section_way, const Item& appending_item);
    private:
        class Trie {
        public:
            Trie();
            Trie(const Trie& other);
            Trie& operator=(const Trie& other);
            ~Trie();

            void AddItem(const std::vector<std::string>& section_way, const Item& appending_item);
        private:
            struct Node {
                std::map<std::string, Node*> next_sections;
                std::map<std::string, Item> items;

                Node();
                Node(const Node& other);
                Node& operator=(const Node& other);
                ~Node();
            };

            Node* root_;
        } tree_;

        bool successful_parse_;
    };

    std::pair<std::any, bool> ConstructValueArray(const std::string& value);
    bool Update(Parser& parser, const std::vector<std::string>& current_sections, std::string& current_key, std::string& current_value);
    Parser parse(const std::filesystem::path& path);
    Parser parse(const std::string& str);
}
