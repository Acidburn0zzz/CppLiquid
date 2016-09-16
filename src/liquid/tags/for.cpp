#include "for.hpp"
#include "parser.hpp"
#include "context.hpp"
#include "template.hpp"
#include "drop.hpp"

namespace Liquid {
    
    class ForloopDrop : public Drop {
    public:
        ForloopDrop(int length)
            : length_(length)
            , index_(0)
        {
        }
        
        int index() const {
            return index_ + 1;
        }
        
        int index0() const {
            return index_;
        }
        
        int rindex() const {
            return length_ - index_;
        }
        
        int rindex0() const {
            return length_ - index_ - 1;
        }
        
        bool first() const {
            return index_ == 0;
        }
        
        bool last() const {
            return index_ == length_ - 1;
        }
        
        int length() const {
            return length_;
        }
        
        void increment() {
            ++index_;
        }
        
    protected:
        virtual Data load(const QString& key) const override {
            if (key == "index") {
                return index();
            } else if (key == "index0") {
                return index0();
            } else if (key == "rindex") {
                return rindex();
            } else if (key == "rindex0") {
                return rindex0();
            } else if (key == "first") {
                return first();
            } else if (key == "last") {
                return last();
            } else if (key == "length") {
                return length();
            } else {
                return Drop::load(key);
            }
        }
        
    private:
        int length_;
        int index_;
    };
    
}

Liquid::ForTag::ForTag(const Context& context, const QStringRef& tagName, const QStringRef& markup)
    : BlockTag(context, tagName, markup)
    , range_(false)
{
    Parser parser(markup);
    varName_ = parser.consume(Token::Type::Id);
    if (parser.consume(Token::Type::Id) != "in") {
        throw std::string("Syntax Error in 'for loop' - Valid syntax: for [item] in [collection]");
    }
    if (parser.look(Token::Type::OpenRound)) {
        range_ = true;
        (void)parser.consume();
        rangeStart_ = Expression::parse(parser);
        (void)parser.consume(Token::Type::DotDot);
        rangeEnd_ = Expression::parse(parser);
        (void)parser.consume(Token::Type::CloseRound);
    } else {
        collection_ = Expression::parse(parser);
    }
    reversed_ = parser.consumeId("reversed");
    while (parser.look(Token::Type::Id)) {
        const QStringRef attr = parser.consume();
        (void)parser.consume(Token::Type::Colon);
        const Expression value = Expression::parse(parser);
        if (attr == "offset") {
            offset_ = value;
        } else if (attr == "limit") {
            limit_ = value;
        } else {
            throw std::string("Invalid attribute in for loop. Valid attributes are limit and offset");
        }
    }
    (void)parser.consume(Token::Type::EndOfString);
}

void Liquid::ForTag::parse(const Context& context, Tokenizer& tokenizer)
{
    if (!parseBody(context, &body_, tokenizer)) {
        return;
    }
    (void)parseBody(context, &elseBlock_, tokenizer);
}

namespace Liquid {

class ForLoop {
public:
    using Item = std::function<Data(int i)>;
    
    ForLoop(const Item& item, BlockBody& body, const QString& varName, int start, int end, const Data& limit, const Data& offset, bool reversed)
        : item_(item)
        , body_(body)
        , varName_(varName)
        , start_(offset.isNumber() ? offset.toInt() : start)
        , end_(limit.isNumber() ? start_ + (limit.toInt() - 1) : end)
        , len_((end_ - start_) + 1)
        , empty_(end_ < start_)
        , reversed_(reversed)
        , forloop_(std::make_shared<ForloopDrop>(len_))
        , index0_(0)
    {
    }
    
    bool empty() const {
        return empty_;
    }
    
    QString render(Context& context) {
        Context::Interrupt interrupt;
        context.data().insert("forloop", Liquid::Data{forloop_});
        if (reversed_) {
            for (int i = end_; i >= start_; --i, ++index0_) {
                if (body(context, i, interrupt)) {
                    if (interrupt == Context::Interrupt::Break) {
                        break;
                    } else if (interrupt == Context::Interrupt::Continue) {
                        continue;
                    }
                }
            }
        } else {
            for (int i = start_; i <= end_; ++i, ++index0_) {
                if (body(context, i, interrupt)) {
                    if (interrupt == Context::Interrupt::Break) {
                        break;
                    } else if (interrupt == Context::Interrupt::Continue) {
                        continue;
                    }
                }
            }
        }
        return output_;
    }
    
    bool body(Context& context, int i, Context::Interrupt& interrupt) {
        context.data().insert(varName_, item_(i));
        output_ += body_.render(context);
        forloop_->increment();
        
        if (context.haveInterrupt()) {
            interrupt = context.pop_interrupt();
            return true;
        }
        
        return false;
    }
    
private:
    const Item& item_;
    BlockBody& body_;
    const QString varName_;
    const int start_;
    const int end_;
    const int len_;
    const bool empty_;
    const bool reversed_;
    std::shared_ptr<ForloopDrop> forloop_;
    int index0_;
    QString output_;
};

}

QString Liquid::ForTag::render(Context& context)
{
    Data& data = context.data();
    int start;
    int end;
    ForLoop::Item item;
    if (range_) {
        start = rangeStart_.evaluate(data).toInt();
        end = rangeEnd_.evaluate(data).toInt();
        item = [](int i) {
            return i;
        };
    } else {
        const Data& collection = collection_.evaluate(data);
        start = 0;
        end = static_cast<int>(collection.size()) - 1;
        item = [&collection](int i) {
            return collection.at(static_cast<size_t>(i));
        };
    }
    ForLoop loop(item, body_, varName_.toString(), start, end, limit_.evaluate(data), offset_.evaluate(data), reversed_);
    if (loop.empty()) {
        return elseBlock_.render(context);
    }
    return loop.render(context);
}

void Liquid::ForTag::handleUnknownTag(const QStringRef& tagName, const QStringRef& markup, Tokenizer& tokenizer)
{
    if (tagName != "else") {
        BlockTag::handleUnknownTag(tagName, markup, tokenizer);
    }
}


#ifdef TESTS

#include "tests.hpp"

TEST_CASE("Liquid::For") {
    
    SECTION("For") {
        CHECK_TEMPLATE_DATA_RESULT(
            "{%for item in array%} yo {%endfor%}",
            " yo  yo  yo  yo ",
            (Liquid::Data::Hash{{"array", Liquid::Data::Array{1, 2, 3, 4}}})
        );
        CHECK_TEMPLATE_DATA_RESULT(
            "{%for item in array%}yo{%endfor%}",
            "yoyo",
            (Liquid::Data::Hash{{"array", Liquid::Data::Array{1, 2}}})
        );
        CHECK_TEMPLATE_DATA_RESULT(
            "{%for item in array%} yo {%endfor%}",
            " yo ",
            (Liquid::Data::Hash{{"array", Liquid::Data::Array{1}}})
        );
        CHECK_TEMPLATE_DATA_RESULT(
            "{%for item in array%}{%endfor%}",
            "",
            (Liquid::Data::Hash{{"array", Liquid::Data::Array{1, 2}}})
        );
    }

    SECTION("ForRange") {
        CHECK_TEMPLATE_RESULT(
            "{%for i in (1..2) %}{% assign a = 'variable'%}{% endfor %}{{a}}",
            "variable"
        );
        CHECK_TEMPLATE_RESULT(
            "{%for item in (1..3) %} {{item}} {%endfor%}",
            " 1  2  3 "
        );
        CHECK_TEMPLATE_DATA_RESULT(
            "{%for item in (1..foobar) %} {{item}} {%endfor%}",
            " 1  2  3 ",
            (Liquid::Data::Hash{{"foobar", 3}})
        );
        CHECK_TEMPLATE_DATA_RESULT(
            "{%for item in (1..foobar.value) %} {{item}} {%endfor%}",
            " 1  2  3 ",
            (Liquid::Data::Hash{{"foobar", Liquid::Data::Hash{{"value", 3}}}})
        );
    }
    
    SECTION("ForReversed") {
        CHECK_TEMPLATE_RESULT(
            "{%for item in (1..3) reversed %} {{item}} {%endfor%}",
            " 3  2  1 "
        );
        CHECK_TEMPLATE_DATA_RESULT(
            "{%for item in array reversed %}{{item}}{%endfor%}",
            "321",
            (Liquid::Data::Hash{{"array", Liquid::Data::Array{1, 2, 3}}})
        );
    }
    
    SECTION("ForVariable") {
        CHECK_TEMPLATE_DATA_RESULT(
            "{%for item in array%} {{item}} {%endfor%}",
            " 1  2  3 ",
            (Liquid::Data::Hash{{"array", Liquid::Data::Array{1, 2, 3}}})
        );
        CHECK_TEMPLATE_DATA_RESULT(
            "{%for item in array%}{{item}}{%endfor%}",
            "123",
            (Liquid::Data::Hash{{"array", Liquid::Data::Array{1, 2, 3}}})
        );
        CHECK_TEMPLATE_DATA_RESULT(
            "{% for item in array %}{{item}}{% endfor %}",
            "123",
            (Liquid::Data::Hash{{"array", Liquid::Data::Array{1, 2, 3}}})
        );
        CHECK_TEMPLATE_DATA_RESULT(
            "{%for item in array%}{{item}}{%endfor%}",
            "abcd",
            (Liquid::Data::Hash{{"array", Liquid::Data::Array{"a", "b", "c", "d"}}})
        );
        CHECK_TEMPLATE_DATA_RESULT(
            "{%for item in array%}{{item}}{%endfor%}",
            "a b c",
            (Liquid::Data::Hash{{"array", Liquid::Data::Array{"a", " ", "b", " ", "c"}}})
        );
        CHECK_TEMPLATE_DATA_RESULT(
            "{%for item in array%}{{item}}{%endfor%}",
            "abc",
            (Liquid::Data::Hash{{"array", Liquid::Data::Array{"a", "", "b", "", "c"}}})
        );
    }
    
    SECTION("ForloopObject") {
        Liquid::Data::Hash hash;
        hash["array"] = Liquid::Data::Array{1, 2, 3};
        CHECK_TEMPLATE_DATA_RESULT(
            "{%for item in array%} {{forloop.index}}/{{forloop.length}} {%endfor%}",
            " 1/3  2/3  3/3 ",
            hash
        );
        CHECK_TEMPLATE_DATA_RESULT(
            "{%for item in array%} {{forloop.index}} {%endfor%}",
            " 1  2  3 ",
            hash
        );
        CHECK_TEMPLATE_DATA_RESULT(
            "{%for item in array%} {{forloop.index0}} {%endfor%}",
            " 0  1  2 ",
            hash
        );
        CHECK_TEMPLATE_DATA_RESULT(
            "{%for item in array%} {{forloop.rindex0}} {%endfor%}",
            " 2  1  0 ",
            hash
        );
        CHECK_TEMPLATE_DATA_RESULT(
            "{%for item in array%} {{forloop.rindex}} {%endfor%}",
            " 3  2  1 ",
            hash
        );
        CHECK_TEMPLATE_DATA_RESULT(
            "{%for item in array%} {{forloop.first}} {%endfor%}",
            " true  false  false ",
            hash
        );
        CHECK_TEMPLATE_DATA_RESULT(
            "{%for item in array%} {{forloop.last}} {%endfor%}",
            " false  false  true ",
            hash
        );
    }
    
    SECTION("ForloopObjectReversed") {
        Liquid::Data::Hash hash;
        hash["array"] = Liquid::Data::Array{1, 2, 3};
        CHECK_TEMPLATE_DATA_RESULT(
            "{%for item in array reversed%} {{forloop.index}}/{{forloop.length}} {%endfor%}",
            " 1/3  2/3  3/3 ",
            hash
        );
        CHECK_TEMPLATE_DATA_RESULT(
            "{%for item in array reversed%} {{forloop.index}} {%endfor%}",
            " 1  2  3 ",
            hash
        );
        CHECK_TEMPLATE_DATA_RESULT(
            "{%for item in array reversed%} {{forloop.index0}} {%endfor%}",
            " 0  1  2 ",
            hash
        );
        CHECK_TEMPLATE_DATA_RESULT(
            "{%for item in array reversed%} {{forloop.rindex0}} {%endfor%}",
            " 2  1  0 ",
            hash
        );
        CHECK_TEMPLATE_DATA_RESULT(
            "{%for item in array reversed%} {{forloop.rindex}} {%endfor%}",
            " 3  2  1 ",
            hash
        );
        CHECK_TEMPLATE_DATA_RESULT(
            "{%for item in array reversed%} {{forloop.first}} {%endfor%}",
            " true  false  false ",
            hash
        );
        CHECK_TEMPLATE_DATA_RESULT(
            "{%for item in array reversed%} {{forloop.last}} {%endfor%}",
            " false  false  true ",
            hash
        );
    }
    
    SECTION("ForloopObjectRange") {
        CHECK_TEMPLATE_RESULT(
            "{%for item in (1..3)%} {{forloop.index}}/{{forloop.length}} {%endfor%}",
            " 1/3  2/3  3/3 "
        );
        CHECK_TEMPLATE_RESULT(
            "{%for item in (1..3)%} {{forloop.index}} {%endfor%}",
            " 1  2  3 "
        );
        CHECK_TEMPLATE_RESULT(
            "{%for item in (1..3)%} {{forloop.index0}} {%endfor%}",
            " 0  1  2 "
        );
        CHECK_TEMPLATE_RESULT(
            "{%for item in (1..3)%} {{forloop.rindex0}} {%endfor%}",
            " 2  1  0 "
        );
        CHECK_TEMPLATE_RESULT(
            "{%for item in (1..3)%} {{forloop.rindex}} {%endfor%}",
            " 3  2  1 "
        );
        CHECK_TEMPLATE_RESULT(
            "{%for item in (1..3)%} {{forloop.first}} {%endfor%}",
            " true  false  false "
        );
        CHECK_TEMPLATE_RESULT(
            "{%for item in (1..3)%} {{forloop.last}} {%endfor%}",
            " false  false  true "
        );
    }

        SECTION("ForloopObjectRangeReversed") {
        CHECK_TEMPLATE_RESULT(
            "{%for item in (1..3) reversed%} {{forloop.index}}/{{forloop.length}} {%endfor%}",
            " 1/3  2/3  3/3 "
        );
        CHECK_TEMPLATE_RESULT(
            "{%for item in (1..3) reversed%} {{forloop.index}} {%endfor%}",
            " 1  2  3 "
        );
        CHECK_TEMPLATE_RESULT(
            "{%for item in (1..3) reversed%} {{forloop.index0}} {%endfor%}",
            " 0  1  2 "
        );
        CHECK_TEMPLATE_RESULT(
            "{%for item in (1..3) reversed%} {{forloop.rindex0}} {%endfor%}",
            " 2  1  0 "
        );
        CHECK_TEMPLATE_RESULT(
            "{%for item in (1..3) reversed%} {{forloop.rindex}} {%endfor%}",
            " 3  2  1 "
        );
        CHECK_TEMPLATE_RESULT(
            "{%for item in (1..3) reversed%} {{forloop.first}} {%endfor%}",
            " true  false  false "
        );
        CHECK_TEMPLATE_RESULT(
            "{%for item in (1..3) reversed%} {{forloop.last}} {%endfor%}",
            " false  false  true "
        );
    }

    // TODO: test_for_and_if

    SECTION("ForElse") {
        CHECK_TEMPLATE_DATA_RESULT(
            "{%for item in array%}+{%else%}-{%endfor%}",
            "+++",
            (Liquid::Data::Hash{{"array", Liquid::Data::Array{1, 2, 3}}})
        );
        CHECK_TEMPLATE_DATA_RESULT(
            "{%for item in array%}+{%else%}-{%endfor%}",
            "-",
            (Liquid::Data::Hash{{"array", Liquid::Data::Array{}}})
        );
        CHECK_TEMPLATE_DATA_RESULT(
            "{%for item in array%}+{%else%}-{%endfor%}",
            "-",
            (Liquid::Data::Hash{{"array", nullptr}})
        );
    }
    
    SECTION("ForLimiting") {
        Liquid::Data::Hash hash;
        hash["array"] = Liquid::Data::Array{1, 2, 3, 4, 5, 6, 7, 8, 9, 0};
        CHECK_TEMPLATE_DATA_RESULT(
            "{%for i in array limit:2 %}{{ i }}{%endfor%}",
            "12",
            hash
        );
        CHECK_TEMPLATE_DATA_RESULT(
            "{%for i in array limit:4 %}{{ i }}{%endfor%}",
            "1234",
            hash
        );
        CHECK_TEMPLATE_DATA_RESULT(
            "{%for i in array limit:4 offset:2 %}{{ i }}{%endfor%}",
            "3456",
            hash
        );
        CHECK_TEMPLATE_DATA_RESULT(
            "{%for i in array limit: 4 offset: 2 %}{{ i }}{%endfor%}",
            "3456",
            hash
        );
        hash["limit"] = 2;
        hash["offset"] = 2;
        CHECK_TEMPLATE_DATA_RESULT(
            "{%for i in array limit: limit offset: offset %}{{ i }}{%endfor%}",
            "34",
            hash
        );
        CHECK_TEMPLATE_DATA_RESULT(
            "{%for i in array offset:7 %}{{ i }}{%endfor%}",
            "890",
            hash
        );
    }

    SECTION("ForLimitingRange") {
        CHECK_TEMPLATE_RESULT(
            "{%for i in (0..9) limit:2 %}{{ i }}{%endfor%}",
            "01"
        );
        CHECK_TEMPLATE_RESULT(
            "{%for i in (0..9) limit:4 %}{{ i }}{%endfor%}",
            "0123"
        );
        CHECK_TEMPLATE_RESULT(
            "{%for i in (0..9) limit:4 offset:2 %}{{ i }}{%endfor%}",
            "2345"
        );
        CHECK_TEMPLATE_RESULT(
            "{%for i in (0..9) limit: 4 offset: 2 %}{{ i }}{%endfor%}",
            "2345"
        );
        CHECK_TEMPLATE_DATA_RESULT(
            "{%for i in (0..9) limit: limit offset: offset %}{{ i }}{%endfor%}",
            "23",
            (Liquid::Data::Hash{{"limit", 2}, {"offset", 2}})
        );
        CHECK_TEMPLATE_DATA_RESULT(
            "{%for i in (0..9) offset:7 %}{{ i }}{%endfor%}",
            "789",
            (Liquid::Data::Hash{{"offset", 2}})
        );
    }
    
    SECTION("ForNested") {
        Liquid::Data::Hash hash;
        hash["array"] = Liquid::Data::Array{Liquid::Data::Array{1, 2}, Liquid::Data::Array{3, 4}, Liquid::Data::Array{5, 6}};
        CHECK_TEMPLATE_DATA_RESULT(
            "{%for item in array%}{%for i in item%}{{ i }}{%endfor%}{%endfor%}",
            "123456",
            hash
        );
    }
    
    // TODO: test_pause_resume*
    
    SECTION("ForBreak") {
        Liquid::Data::Hash hash;
        hash["array"] = Liquid::Data::Hash{{"items", Liquid::Data::Array{1, 2, 3, 4, 5, 6, 7, 8, 9, 10}}};
        CHECK_TEMPLATE_DATA_RESULT(
            "{% for i in array.items %}{% break %}{% endfor %}",
            "",
            hash
        );
        CHECK_TEMPLATE_DATA_RESULT(
            "{% for i in array.items %}{{ i }}{% break %}{% endfor %}",
            "1",
            hash
        );
        CHECK_TEMPLATE_DATA_RESULT(
            "{% for i in array.items %}{% break %}{{ i }}{% endfor %}",
            "",
            hash
        );
        
        // TODO for/if
    }

    SECTION("ForContinue") {
        Liquid::Data::Hash hash;
        hash["array"] = Liquid::Data::Hash{{"items", Liquid::Data::Array{1, 2, 3, 4, 5}}};
        CHECK_TEMPLATE_DATA_RESULT(
            "{% for i in array.items %}{% continue %}{% endfor %}",
            "",
            hash
        );
        CHECK_TEMPLATE_DATA_RESULT(
            "{% for i in array.items %}{{ i }}{% continue %}{% endfor %}",
            "12345",
            hash
        );
        CHECK_TEMPLATE_DATA_RESULT(
            "{% for i in array.items %}{% continue %}{{ i }}{% endfor %}",
            "",
            hash
        );
        
        // TODO for/if
    }
    
}

#endif
