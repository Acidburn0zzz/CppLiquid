#include "variable.hpp"
#include <QDebug>

Liquid::Variable::Variable(const QStringRef& input)
{
    Parser parser(input);
    exp_ = Expression::parse(parser);
    
    while (parser.look(Token::Type::Pipe)) {
        (void)parser.consume();
        const QStringRef name = parser.consume(Token::Type::Id);
        std::vector<Expression> args;
        if (parser.look(Token::Type::Colon)) {
            (void)parser.consume();
            for (;;) {
                args.push_back(Expression::parse(parser));
                if (!parser.look(Token::Type::Comma)) {
                    break;
                }
                (void)parser.consume();
            }
        }
        filters_.emplace_back(name, args);
    }
}

const Liquid::Data& Liquid::Variable::evaluate(const Context& context) const
{
    const Data& result = exp_.evaluate(context.data());
    if (filters_.empty()) {
        return result;
    }
    Data value = result;
    for (const auto& filter : filters_) {
        const auto& args = filter.args();
        std::vector<Data> evaluatedArgs;
        for (const auto& arg : args) {
            evaluatedArgs.push_back(arg.evaluate(context.data()));
        }
        const auto filterIter = context.filters().find(filter.name().toString().toStdString());
        if (filterIter == context.filters().end()) {
            throw QString("Unknown filter %1").arg(filter.name().toString()).toStdString();
        }
        value = filterIter->second(value, evaluatedArgs);
    }
    cached_ = value;
    return cached_;
}



#ifdef TESTS

#include "catch.hpp"

TEST_CASE("Liquid::Variable") {
    

}

#endif