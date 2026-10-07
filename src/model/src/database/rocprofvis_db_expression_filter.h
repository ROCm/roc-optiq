// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <variant>
#include <functional>
#include <cctype>
#include <stdexcept>
#include <cmath>
#include <algorithm>
#include <regex>
#include "spdlog/spdlog.h"

namespace RocProfVis
{
namespace DataModel
{

    class Tokenizer
    {
    public:
        explicit Tokenizer(const std::string& text)
            : m_text(text), m_pos(0) {}

        inline bool IEquals(const std::string& a, const std::string& b, size_t n)
        {
            if (a.size() < n || b.size() < n) return false;

            for (size_t i = 0; i < n; ++i)
            {
                if (std::toupper(static_cast<unsigned char>(a[i])) !=
                    std::toupper(static_cast<unsigned char>(b[i])))
                    return false;
            }
            return true;
        }

        void SkipSpaces()
        {
            while (m_pos < m_text.size() && std::isspace((unsigned char)m_text[m_pos]))
                ++m_pos;
        }

        // Peek next character (returns 0 if end)
        char Peek() const
        {
            if (m_pos < m_text.size())
                return m_text[m_pos];
            return 0;
        }

        // Get next character (advance position)
        char Get()
        {
            if (m_pos < m_text.size())
                return m_text[m_pos++];
            return 0;
        }

        std::string GetText()
        {
            SkipSpaces();
            size_t start = m_pos;
            while (m_pos < m_text.size() && m_text[m_pos] != ' ')
                ++m_pos;
            return m_text.substr(start, m_pos - start);
        }

        // Return a single word (letters only)
        std::string GetWord()
        {
            SkipSpaces();
            size_t start = m_pos;
            while (m_pos < m_text.size() && std::isalpha(static_cast<unsigned char>(m_text[m_pos])))
                ++m_pos;
            return m_text.substr(start, m_pos - start);
        }

        // Try to match a specific symbol or keyword (e.g., "(", ")", "AND", etc.)
        bool Match(const std::string& token)
        {
            SkipSpaces();
            size_t len = token.size();

            if (m_pos + len <= m_text.size() && IEquals(m_text.substr(m_pos, len), token, len))
            {
                // Ensure it's not a prefix of a longer word (for keywords)
                if (std::isalpha(static_cast<unsigned char>(token.back())) &&
                    m_pos + len < m_text.size() &&
                    std::isalpha(static_cast<unsigned char>(m_text[m_pos + len])))
                {
                    return false;
                }

                m_pos += len;
                SkipSpaces();
                return true;
            }
            return false;
        }

        // Puts a token (or operator) back into the stream
        void PutBack(const std::string& token)
        {
            if (token.empty()) return;
            m_pos = (m_pos >= token.size()) ? m_pos - token.size() : 0;
        }

        // --- Common token extractors (simplified) ---

        std::string GetIdentifier()
        {
            SkipSpaces();
            size_t start = m_pos;
            while (m_pos < m_text.size() && (std::isalnum((unsigned char)m_text[m_pos]) || m_text[m_pos] == '_'))
                ++m_pos;
            return m_text.substr(start, m_pos - start);
        }

        // A number literal: an optional '-', digits with at most one '.', and an optional
        // exponent. Only called where an operand starts, so a '-' here is a sign; a '-'
        // between two operands has already been taken by ParseArithmetic as a subtraction.
        std::string GetNumber()
        {
            SkipSpaces();
            size_t pos = m_pos;
            if (pos < m_text.size() && m_text[pos] == '-')
            {
                ++pos;
            }
            size_t digits = 0;
            while (pos < m_text.size() && std::isdigit((unsigned char)m_text[pos]))
            {
                ++pos;
                ++digits;
            }
            if (pos < m_text.size() && m_text[pos] == '.')
            {
                ++pos;
                while (pos < m_text.size() && std::isdigit((unsigned char)m_text[pos]))
                {
                    ++pos;
                    ++digits;
                }
            }
            if (digits == 0)
            {
                return "";
            }
            if (pos < m_text.size() && (m_text[pos] == 'e' || m_text[pos] == 'E'))
            {
                size_t exponent = pos + 1;
                if (exponent < m_text.size() && (m_text[exponent] == '+' || m_text[exponent] == '-'))
                {
                    ++exponent;
                }
                if (exponent < m_text.size() && std::isdigit((unsigned char)m_text[exponent]))
                {
                    while (exponent < m_text.size() && std::isdigit((unsigned char)m_text[exponent]))
                    {
                        ++exponent;
                    }
                    pos = exponent;
                }
            }
            std::string number = m_text.substr(m_pos, pos - m_pos);
            m_pos = pos;
            return number;
        }

        std::string GetHexNumber()
        {
            SkipSpaces();
            size_t start = m_pos;
            if (m_pos + 2 < m_text.size() && m_text[m_pos] == '0' && (m_text[m_pos + 1] == 'x' || m_text[m_pos + 1] == 'X'))
            {
                m_pos += 2;
                while (m_pos < m_text.size() && std::isxdigit((unsigned char)m_text[m_pos]))
                    ++m_pos;
                return m_text.substr(start, m_pos - start);
            }
            return "";
        }

        std::string GetStringLiteral(bool& empty_string)
        {
            SkipSpaces();
            if (m_pos >= m_text.size())
                return "";

            char quote = m_text[m_pos];
            if (quote != '\'' && quote != '"')
                return "";

            ++m_pos; // skip opening quote

            std::string result;
            while (m_pos < m_text.size())
            {
                char c = m_text[m_pos++];

                if (c == quote)
                {
                    // Escaped quote: '' or ""
                    if (m_pos < m_text.size() && m_text[m_pos] == quote)
                    {
                        result += quote;
                        ++m_pos;
                    }
                    else
                    {
                        break; // closing quote
                    }
                }
                else
                {
                    result += c;
                }
            }

            empty_string = result.empty();
            return result;
        }

        std::string GetOperator()
        {
            SkipSpaces();
            static const std::vector<std::string> ops = {
                "<=", ">=", "<>", "!=", "=", "==", "<", ">", "+", "-", "*", "/", "%", "LIKE"
            };

            for (const auto& op : ops)
            {
                size_t len = op.size();
                if (m_pos + len <= m_text.size() &&
                    IEquals(m_text.substr(m_pos, len), op, len))
                {
                    m_pos += len;
                    return op;
                }
            }
            return "";
        }

        bool End() const { return m_pos >= m_text.size(); }

    private:
        std::string m_text;
        size_t m_pos;
    };

    class FilterExpression {
    public:
        using Value = std::variant<double, std::string>;
        using FunctionResult = std::variant<double, std::string>;
        using FunctionHandler = std::function<FunctionResult(const std::vector<FunctionResult>&)>;

        // Default constructor
        FilterExpression() = default;

        // Copy constructor and assignment
        FilterExpression(const FilterExpression& other);
        FilterExpression& operator=(const FilterExpression& other);

        // Move constructor and assignment (optional but good practice)
        FilterExpression(FilterExpression&& other) noexcept = default;
        FilterExpression& operator=(FilterExpression&& other) noexcept = default;


        enum class Operator { Equal, NotEqual, Less, LessEqual, Greater, GreaterEqual, Like };
        enum class LogicOp { None, And, Or, Not };
        enum class SqlCommand { Column, Count, Avg, Min, Max, Sum};

        struct SqlAggregation {
            std::string column;       
            SqlCommand command;       
            std::string public_name;  
        };

        struct ExprNode {
            enum class Type { ConstantNumber, ConstantString, Column, Add, Sub, Mul, Div, Mod, Function } m_type;
            std::variant<std::string, double> m_data;
            std::unique_ptr<ExprNode> m_left;
            std::unique_ptr<ExprNode> m_right;
            std::vector<std::unique_ptr<ExprNode>> m_args;
            FunctionHandler m_funcHandler = nullptr;

            FunctionResult Evaluate(const std::unordered_map<std::string, Value>& row) const;
        };

        struct Condition {
            std::unique_ptr<ExprNode> m_leftExpr;
            Operator m_op;
            std::unique_ptr<ExprNode> m_rightExpr;
            bool m_negate = false;
            // Character named by LIKE ... ESCAPE 'c', or 0 when the pattern has none.
            char m_escape = 0;
        };

        struct Node {
            std::unique_ptr<Node> m_left;
            std::unique_ptr<Node> m_right;
            LogicOp m_logic = LogicOp::None;
            std::unique_ptr<Condition> m_condition;
        };

        static FilterExpression Parse(const std::string& expr);
        bool Evaluate(const std::unordered_map<std::string, Value>& row) const;
        static void RegisterFunction(const std::string& name, FunctionHandler handler);
        static std::vector<SqlAggregation> ParseAggregationSpec(const std::string& line);
        static bool StartsWithSubstring(const std::string& input, std::string expected);

    private:
        std::unique_ptr<Node> m_root;
        static std::unordered_map<std::string, FunctionHandler> s_functions;

        // Parsing
        static std::unique_ptr<Node> ParseExpression(Tokenizer& tk);
        static std::unique_ptr<Node> ParseTerm(Tokenizer& tk);
        static std::unique_ptr<Node> ParseFactor(Tokenizer& tk);
        static std::unique_ptr<Condition> ParseCondition(Tokenizer& tk);
        static std::unique_ptr<ExprNode> ParseArithmetic(Tokenizer& tk);
        static std::unique_ptr<ExprNode> ParseTermExpr(Tokenizer& tk);
        static std::unique_ptr<ExprNode> ParseFactorExpr(Tokenizer& tk);

        // Helpers
        static bool EvaluateNode(const Node* node, const std::unordered_map<std::string, Value>& row);
        static bool MatchLike(const std::string& text, const std::string& pattern, char escape);

        // Helper to deep copy Node tree
        static std::unique_ptr<Node> CopyNode(const std::unique_ptr<Node>& node);
        static std::unique_ptr<ExprNode> CopyExprNode(const std::unique_ptr<ExprNode>& expr);
        static std::unique_ptr<Condition> CopyCondition(const std::unique_ptr<Condition>& cond);

        static SqlCommand ParseAggrCommand(const std::string& cmd);
        static std::string AggrCommandToString(SqlCommand cmd);
    };

}  // namespace DataModel
}  // namespace RocProfVis