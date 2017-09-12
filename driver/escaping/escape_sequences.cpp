#include "escape_sequences.h"
#include "lexer.h"

using namespace std;

namespace {

string processEscapeSequencesImpl(const StringView seq, Lexer& lex);

string convertFunctionByType(const StringView& typeName) {
    if (typeName == "SQL_BIGINT") {
        return "toInt64";
    }
    if (typeName == "SQL_INTEGER") {
        return "toInt32";
    }
    return string();
}

string processFunction(const StringView seq, Lexer& lex) {
    const Token fn(lex.Consume());

    if (fn.type == Token::CONVERT) {
        if (!lex.Match(Token::LPARENT)) {
            return seq.to_string();
        }

        Token num = lex.Consume();
        if (num.type != Token::NUMBER) {
            return seq.to_string();
        }
        if (!lex.Match(Token::COMMA)) {
            return seq.to_string();
        }
        Token type = lex.Consume();
        if (type.type != Token::IDENT) {
            return seq.to_string();
        }

        string func = convertFunctionByType(type.literal.to_string());

        if (!func.empty()) {
            if (!lex.Match(Token::RPARENT)) {
                return seq.to_string();
            }
            return func + "(" + num.literal.to_string() + ")";
        }

    } else if (fn.type == Token::CONCAT) {
        string result = "concat";

        while (true) {
            const Token tok(lex.Peek());

            if (tok.type == Token::RCURLY) {
                break;
            } else if (tok.type == Token::LCURLY) {
                result += processEscapeSequencesImpl(seq, lex);
            } else if (tok.type == Token::EOS || tok.type == Token::INVALID) {
                break;
            } else {
                result += tok.literal.to_string();
                lex.Consume();
            }
        }

        return result;
    }

    return seq.to_string();
}

string processDate(const StringView seq, Lexer& lex) {
    Token data = lex.Consume(Token::STRING);
    if (data.isInvalid()) {
        return seq.to_string();
    } else {
        return string("toDate(") + data.literal.to_string() + ")";
    }
}

string processDateTime(const StringView seq, Lexer& lex) {
    Token data = lex.Consume(Token::STRING);
    if (data.isInvalid()) {
        return seq.to_string();
    } else {
        return string("toDateTime(") + data.literal.to_string() + ")";
    }
}

string processEscapeSequencesImpl(const StringView seq, Lexer& lex) {
    string result;

    if (!lex.Match(Token::LCURLY)) {
        return seq.to_string();
    }

    while (true) {
        const Token tok(lex.Consume());

        switch (tok.type) {
            case Token::FN:
                result += processFunction(seq, lex);
                break;

            case Token::D:
                result += processDate(seq, lex);
                break;
            case Token::TS:
                result += processDateTime(seq, lex);
                break;

            // End of escape sequence
            case Token::RCURLY:
                return result;

            // Unimplemented
            case Token::T:
            default:
                return seq.to_string();
        }
    };
}

string processEscapeSequences(const StringView seq) {
    Lexer lex(seq);
    return processEscapeSequencesImpl(seq, lex);
}

} // namespace

std::string replaceEscapeSequences(const std::string & query)
{
    const char* p = query.c_str();
    const char* end = p + query.size();
    const char* st = p;
    int level = 0;
    std::string ret;

    while (p != end) {
        switch (*p) {
            case '{':
                if (level == 0) {
                    if (st < p) {
                        ret += std::string(st, p);
                    }
                    st = p;
                }
                level++;
                break;

            case '}':
                if (level == 0) {
                    // TODO unexpected '}'
                    return query;
                }
                if (--level == 0) {
                    ret += processEscapeSequences(StringView(st, p + 1));
                    st = p + 1;
                }
                break;
        }

        ++p;
    }

    if (st < p) {
        ret += std::string(st, p);
    }

    return ret;
}
