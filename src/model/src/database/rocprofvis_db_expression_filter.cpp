// Copyright (c) 2025 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "rocprofvis_db_expression_filter.h"
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <regex>
#include <sstream>


namespace RocProfVis
{
namespace DataModel
{
    std::unordered_map<std::string, FilterExpression::FunctionHandler> FilterExpression::s_functions;

    // ---------------- Evaluation ----------------

    FilterExpression::FunctionResult FilterExpression::ExprNode::Evaluate(const std::unordered_map<std::string, Value>& row) const {
        switch (m_type) {
        case Type::ConstantNumber: return std::get<double>(m_data);
        case Type::ConstantString: return std::get<std::string>(m_data);
        case Type::Column: {
            auto it = row.find(std::get<std::string>(m_data));
            if (it == row.end()) 
                throw std::runtime_error("Unknown column name");
            return it->second;
        }
        case Type::Add:
        case Type::Sub:
        case Type::Mul:
        case Type::Div:
        case Type::Mod:
        {
            double lhs = std::get<double>(m_left->Evaluate(row));
            double rhs = std::get<double>(m_right->Evaluate(row));
            switch (m_type) {
            case Type::Add: return lhs + rhs;
            case Type::Sub: return lhs - rhs;
            case Type::Mul: return lhs * rhs;
            case Type::Div: return rhs != 0.0 ? lhs / rhs : 0.0;
            case Type::Mod: return rhs != 0.0 ? (size_t)lhs % (size_t)rhs : 0.0;
            default: return 0.0;
            }
        }
        case Type::Function: {
            std::vector<FunctionResult> args;
            args.reserve(m_args.size());
            for (auto& a : m_args) args.push_back(a->Evaluate(row));
            if (m_funcHandler) return m_funcHandler(args);
            throw std::runtime_error("Unknown function");
        }
        default: return 0.0;
        }
    }

    // ---------------- Helpers ----------------

    bool FilterExpression::MatchLike(const std::string& text, const std::string& pattern) {
        std::string regexPattern;
        for (char c : pattern) {
            if (c == '%') regexPattern += ".*";
            else if (c == '_') regexPattern += '.';
            else if (std::strchr(".^$|()[]*+?\\", c)) { regexPattern += '\\'; regexPattern += c; }
            else regexPattern += c;
        }
        return std::regex_match(text, std::regex(regexPattern, std::regex_constants::icase));
    }


    bool FilterExpression::EvaluateNode(const Node* node, const std::unordered_map<std::string, Value>& row) {
        if (!node) return true;

        if (node->m_condition) {
            const auto& cond = node->m_condition;
            auto lhs = cond->m_leftExpr->Evaluate(row);
            auto rhs = cond->m_rightExpr->Evaluate(row);

            if (cond->m_op == Operator::Like) {
                auto lhs_str = std::get<std::string>(lhs);  
                auto rhs_str = std::get<std::string>(rhs);   
                bool result = MatchLike(lhs_str, rhs_str);
                if (cond->m_negate)
                    result = !result;
                return result;
            } else
            if (std::holds_alternative<double>(lhs) && std::holds_alternative<double>(rhs))
            {
                double lv = std::get<double>(lhs);
                double rv = std::get<double>(rhs);

                switch (cond->m_op) {
                case Operator::Equal: return lv == rv;
                case Operator::NotEqual: return lv != rv;
                case Operator::Less: return lv < rv;
                case Operator::LessEqual: return lv <= rv;
                case Operator::Greater: return lv > rv;
                case Operator::GreaterEqual: return lv >= rv;
                default: return false;
                }
            } else
            if (std::holds_alternative<std::string>(lhs) && std::holds_alternative<std::string>(rhs) &&  
                (cond->m_op == Operator::Equal || cond->m_op == Operator::NotEqual))
            {
                auto lhs_str = std::get<std::string>(lhs);  
                auto rhs_str = std::get<std::string>(rhs);
                return cond->m_op == Operator::Equal ? lhs_str == rhs_str : lhs_str != rhs_str;
            }

            if (lhs.index() != rhs.index()) {

                return false;
            }


        }

        switch (node->m_logic) {
        case LogicOp::And: return EvaluateNode(node->m_left.get(), row) && EvaluateNode(node->m_right.get(), row);
        case LogicOp::Or: return EvaluateNode(node->m_left.get(), row) || EvaluateNode(node->m_right.get(), row);
        case LogicOp::Not: return !EvaluateNode(node->m_left.get(), row);
        default: return EvaluateNode(node->m_left.get(), row);
        }
    }


    bool FilterExpression::Evaluate(const std::unordered_map<std::string, Value>& row) const {
        return EvaluateNode(m_root.get(), row);
    }

    // ---------------- Parsing ----------------

    FilterExpression FilterExpression::Parse(const std::string& expr) {
        Tokenizer tk(expr);
        FilterExpression fe;
        fe.m_root = ParseExpression(tk);
        return fe;
    }

    // --- Expression Parsing ---

    std::unique_ptr<FilterExpression::Node> FilterExpression::ParseExpression(Tokenizer& tk)
    {
        auto node = ParseTerm(tk);
        tk.SkipSpaces();

        while (true)
        {
            std::string word = tk.GetWord();
            if (word.empty())
                break;

            std::string upper = word;
            std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

            if (upper == "OR")
            {
                auto right = ParseTerm(tk);
                auto parent = std::make_unique<Node>();
                parent->m_logic = LogicOp::Or;
                parent->m_left = std::move(node);
                parent->m_right = std::move(right);
                node = std::move(parent);
            }
            else
            {
                tk.PutBack(word);
                break;
            }
        }
        return node;
    }

    std::unique_ptr<FilterExpression::Node> FilterExpression::ParseTerm(Tokenizer& tk)
    {
        auto node = ParseFactor(tk);
        tk.SkipSpaces();

        while (true)
        {
            std::string word = tk.GetWord();
            if (word.empty())
                break;

            std::string upper = word;
            std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

            if (upper == "AND")
            {
                auto right = ParseFactor(tk);
                auto parent = std::make_unique<Node>();
                parent->m_logic = LogicOp::And;
                parent->m_left = std::move(node);
                parent->m_right = std::move(right);
                node = std::move(parent);
            }
            else
            {
                tk.PutBack(word);
                break;
            }
        }
        return node;
    }

    std::unique_ptr<FilterExpression::Node> FilterExpression::ParseFactor(Tokenizer& tk)
    {
        tk.SkipSpaces();

        if (tk.Match("NOT"))
        {
            auto node = std::make_unique<Node>();
            node->m_logic = LogicOp::Not;
            node->m_left = ParseFactor(tk);  // NOT applies to next factor (expression in brackets or condition)
            return node;
        }

        if (tk.Peek() == '(')
        {
            tk.Get(); // consume '('
            auto node = ParseExpression(tk);
            tk.SkipSpaces();

            if (tk.Peek() != ')')
                throw std::runtime_error("Expected ')' to close expression group");
            tk.Get(); // consume ')'

            return node;
        }

        auto cond = ParseCondition(tk);
        auto node = std::make_unique<Node>();
        node->m_condition = std::move(cond);
        node->m_logic = LogicOp::None;
        return node;
    }

    std::unique_ptr<FilterExpression::Condition> FilterExpression::ParseCondition(Tokenizer& tk)
    {
        auto cond = std::make_unique<Condition>();

        // 1️ Parse left expression — can now be an arithmetic expression, not just a column
        cond->m_leftExpr = ParseArithmetic(tk);
        if (!cond->m_leftExpr)
            throw std::runtime_error("Expected left-hand expression in condition");

        // 2️ Parse operator
        std::string op = tk.GetOperator();
        if (op.empty()) {
            // handle case like "name NOT LIKE ...", so look for keyword "NOT"
            std::string maybeNot = tk.GetIdentifier();
            std::transform(maybeNot.begin(), maybeNot.end(), maybeNot.begin(), ::toupper);

            if (maybeNot == "NOT") {
                std::string next = tk.GetIdentifier();
                std::transform(next.begin(), next.end(), next.begin(), ::toupper);
                if (next == "LIKE") {
                    cond->m_op = Operator::Like;
                    cond->m_negate = true;  // add new flag to Condition
                } else {
                    throw std::runtime_error("Expected LIKE after NOT");
                }
            } else {
                throw std::runtime_error("Expected operator after column ");
            }
        }
        else {

            std::transform(op.begin(), op.end(), op.begin(), ::toupper);
            if (op == "=" || op == "==") cond->m_op = Operator::Equal;
            else if (op == "!=")              cond->m_op = Operator::NotEqual;
            else if (op == "<")               cond->m_op = Operator::Less;
            else if (op == "<=")              cond->m_op = Operator::LessEqual;
            else if (op == ">")               cond->m_op = Operator::Greater;
            else if (op == ">=")              cond->m_op = Operator::GreaterEqual;
            else if (op == "LIKE")            cond->m_op = Operator::Like;
            else
                throw std::runtime_error("Unknown operator: " + op);
        }

        // 3️ Parse right-hand side — can also be arithmetic or literal
        cond->m_rightExpr = ParseArithmetic(tk);
        if (!cond->m_rightExpr)
            throw std::runtime_error("Expected right-hand expression after operator: " + op);

        return cond;
    }

    // ---------------- RegisterFunction ----------------

    void FilterExpression::RegisterFunction(const std::string& name, FunctionHandler handler) {
        std::string upper = name;
        std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
        s_functions[upper] = handler;
    }

    // ------------------------------
    // ParseArithmetic
    // Handles:  expr OR expr OR expr ...
    // ------------------------------
    std::unique_ptr<FilterExpression::ExprNode> FilterExpression::ParseArithmetic(Tokenizer& tk)
    {
        auto left = ParseTermExpr(tk);
        if (!left) return nullptr;

        while (true)
        {
            std::string op = tk.GetOperator();
            if (op == "+" || op == "-")
            {
                auto right = ParseTermExpr(tk);
                if (!right)
                    throw std::runtime_error("Expected right-hand term after operator: " + op);

                auto node = std::make_unique<ExprNode>();
                node->m_type = op == "+" ? ExprNode::Type::Add : ExprNode::Type::Sub;
                node->m_left = std::move(left);
                node->m_right = std::move(right);
                left = std::move(node);
            }
            else
            {
                tk.PutBack(op);
                break;
            }
        }

        return left;
    }


    std::unique_ptr<FilterExpression::ExprNode> FilterExpression::ParseTermExpr(Tokenizer& tk)
    {
        auto left = ParseFactorExpr(tk);
        if (!left) return nullptr;

        while (true)
        {
            std::string op = tk.GetOperator();
            if (op == "*" || op == "/" || op == "%")
            {
                auto right = ParseFactorExpr(tk);
                if (!right)
                    throw std::runtime_error("Expected right-hand factor after operator: " + op);

                auto node = std::make_unique<ExprNode>();
                node->m_type = op == "*" ? ExprNode::Type::Mul : op == "/" ? ExprNode::Type::Div : ExprNode::Type::Mod;
                node->m_left = std::move(left);
                node->m_right = std::move(right);
                left = std::move(node);
            }
            else
            {
                tk.PutBack(op);
                break;
            }
        }

        return left;
    }


    std::unique_ptr<FilterExpression::ExprNode> FilterExpression::ParseFactorExpr(Tokenizer& tk)
    {
        tk.SkipSpaces();

        // Parenthesized subexpression
        if (tk.Match("("))
        {
            auto expr = ParseArithmetic(tk);
            if (!tk.Match(")"))
                throw std::runtime_error("Expected closing parenthesis");
            return expr;
        }

        // String literal
        bool empty_tring = false;
        std::string str = tk.GetStringLiteral(empty_tring);
        if (!str.empty() || empty_tring)
        {
            auto node = std::make_unique<ExprNode>();
            node->m_type = ExprNode::Type::ConstantString;
            node->m_data = str;
            return node;
        }

        // Hexadecimal number
        std::string hex = tk.GetHexNumber();
        if (!hex.empty())
        {
            unsigned long long val = std::stoull(hex, nullptr, 16);
            auto node = std::make_unique<ExprNode>();
            node->m_type = ExprNode::Type::ConstantNumber;
            node->m_data = static_cast<double>(val);
            return node;
        }

        // Numeric literal
        std::string num = tk.GetNumber();
        if (!num.empty())
        {
            auto node = std::make_unique<ExprNode>();
            node->m_type = ExprNode::Type::ConstantNumber;
            node->m_data = std::stod(num);
            return node;
        }

        // Identifier (column or variable)
        std::string ident = tk.GetIdentifier();
        if (!ident.empty())
        {
            auto node = std::make_unique<ExprNode>();
            node->m_type = ExprNode::Type::Column;
            node->m_data = ident;
            return node;
        }

        return nullptr;
    }


    // ---------------- Copy Helpers ----------------

    std::unique_ptr<FilterExpression::ExprNode> FilterExpression::CopyExprNode(const std::unique_ptr<ExprNode>& expr) {
        if (!expr) return nullptr;
        auto copy = std::make_unique<ExprNode>();
        copy->m_type = expr->m_type;
        copy->m_data = expr->m_data;
        copy->m_funcHandler = expr->m_funcHandler;
        copy->m_left = CopyExprNode(expr->m_left);
        copy->m_right = CopyExprNode(expr->m_right);
        copy->m_args.reserve(expr->m_args.size());
        for (auto& a : expr->m_args) copy->m_args.push_back(CopyExprNode(a));
        return copy;
    }

    std::unique_ptr<FilterExpression::Condition> FilterExpression::CopyCondition(const std::unique_ptr<Condition>& cond) {
        if (!cond) return nullptr;
        auto copy = std::make_unique<Condition>();
        copy->m_op = cond->m_op;
        copy->m_leftExpr = CopyExprNode(cond->m_leftExpr);
        copy->m_rightExpr = CopyExprNode(cond->m_rightExpr);
        return copy;
    }

    std::unique_ptr<FilterExpression::Node> FilterExpression::CopyNode(const std::unique_ptr<Node>& node) {
        if (!node) return nullptr;
        auto copy = std::make_unique<Node>();
        copy->m_logic = node->m_logic;
        copy->m_condition = CopyCondition(node->m_condition);
        copy->m_left = CopyNode(node->m_left);
        copy->m_right = CopyNode(node->m_right);
        return copy;
    }

    // ---------------- Copy constructor / assignment ----------------

    FilterExpression::FilterExpression(const FilterExpression& other) {
        m_root = CopyNode(other.m_root);
    }

    FilterExpression& FilterExpression::operator=(const FilterExpression& other) {
        if (this != &other) {
            m_root = CopyNode(other.m_root);
        }
        return *this;
    }

    static inline std::string trim(const std::string& s) {
        auto start = s.find_first_not_of(" \t\n\r");
        auto end = s.find_last_not_of(" \t\n\r");
        return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
    }

    FilterExpression::SqlCommand FilterExpression::ParseAggrCommand(const std::string& cmd) {
        std::string upper;
        upper.reserve(cmd.size());
        for (char c : cmd) upper.push_back(std::toupper(static_cast<unsigned char>(c)));

        if (upper == "COUNT") return SqlCommand::Count;
        if (upper == "AVG")   return SqlCommand::Avg;
        if (upper == "MIN")   return SqlCommand::Min;
        if (upper == "MAX")   return SqlCommand::Max;
        if (upper == "SUM")   return SqlCommand::Sum;
        return SqlCommand::Group;
    }

    std::string FilterExpression::AggrCommandToString(SqlCommand cmd) {
        switch (cmd) {
        case SqlCommand::Count: return "COUNT";
        case SqlCommand::Avg:   return "AVG";
        case SqlCommand::Min:   return "MIN";
        case SqlCommand::Max:   return "MAX";
        case SqlCommand::Sum:   return "SUM";
        default: return "";
        }
    }

    std::vector<FilterExpression::SqlAggregation> FilterExpression::ParseAggregationSpec(const std::string& line) {
        std::vector<SqlAggregation> result;
        std::stringstream ss(line);
        std::string token;

        static const std::regex funcRegex(
            R"(^([A-Za-z_][A-Za-z0-9_]*)\s*\(\s*([^)]+)\s*\)\s*(?:AS\s+([A-Za-z_][A-Za-z0-9_]*))?$)",
            std::regex::icase
        );

        while (std::getline(ss, token, ',')) {
            token = trim(token);
            std::smatch match;

            SqlAggregation agg;

            if (std::regex_match(token, match, funcRegex)) {
                agg.command = ParseAggrCommand(match[1].str());
                agg.column = trim(match[2].str());
                agg.public_name = match[3].matched ? match[3].str()
                    : AggrCommandToString(agg.command) + "_" + agg.column;
            } else {
                agg.command = SqlCommand::Group;
                agg.column = token;
                agg.public_name = token;
            }

            result.push_back(agg);
        }
        return result;
    }

}  // namespace DataModel
}  // namespace RocProfVis
