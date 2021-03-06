#include "capture.hpp"
#include "parser.hpp"
#include "context.hpp"
#include "template.hpp"

Liquid::CaptureTag::CaptureTag(const Context& context, const StringRef& tagName, const StringRef& markup)
    : BlockTag(context, tagName, markup)
{
    Parser parser(markup);
    to_ = parser.consume(Token::Type::Id);
    (void)parser.consume(Token::Type::EndOfString);
}

Liquid::String Liquid::CaptureTag::render(Context& context)
{
    const String output = BlockTag::render(context);
    context.data().insert(to_.toString(), output);
    return "";
}



#ifdef TESTS

#include "tests.hpp"

TEST_CASE("Liquid::Capture") {
    
    SECTION("Capture") {
        CHECK_TEMPLATE_RESULT("{% capture my_variable %}I am being captured.{% endcapture %}{{ my_variable }}", "I am being captured.");
        CHECK_TEMPLATE_DATA_RESULT(
            "{{ var2 }}{% capture var2 %}{{ var }} foo {% endcapture %}{{ var2 }}{{ var2 }}",
            "content foo content foo ",
            (Liquid::Data::Hash{{"var", "content"}})
        );
    }
}

#endif
