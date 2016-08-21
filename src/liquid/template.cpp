#include "template.hpp"
#include "expression.hpp"
#include <QDebug>

#define ABORT(msg) throw QString(msg).toStdString()

void Liquid::Template::parse(const QString& source)
{
    source_ = source;
    components_ = tokenize(source_);
}

std::vector<Liquid::Template::ComponentPtr> Liquid::Template::tokenize(const QString& source) const
{
    std::vector<ComponentPtr> components;
    int lastStartPos = 0;
    const int len = source_.size();
    const int lastCharPos = len - 1;
    const QString endStr[] = {"}}", "%}"};
    
    while (lastStartPos < len) {
        // Look for the next starting object or tag
        const int startPos = source_.indexOf('{', lastStartPos);
        if (startPos == -1 || startPos == lastCharPos) {
            break;
        }
        
        // Check the next character
        const QChar nextChar = source_.at(startPos + 1);
        if (nextChar == '{' || nextChar == '%') {
            // Look for the end of the object or tag
            const bool isObject = nextChar == '{';
            const int endPos = source_.indexOf(endStr[isObject ? 0 : 1], startPos + 2);
            if (endPos == -1) {
                ABORT("Tag not properly terminated");
            }
            
            // Collect any text component before the object or tag
            const int chunkLen = startPos - lastStartPos;
            if (chunkLen > 0) {
                const QStringRef text = source_.midRef(lastStartPos, chunkLen);
                components.push_back(std::make_unique<TextComponent>(text));
            }
            
            // Collect the complete text of the object or tag
            const int tagEndPos = endPos + 2;
            const QStringRef tag = source_.midRef(startPos, tagEndPos - startPos);
            const QStringRef tagTrimmed = tag.mid(2, tag.size() - 4).trimmed();
            if (isObject) {
                components.push_back(std::make_unique<ObjectComponent>(tag, Variable(tagTrimmed)));
            } else {
                components.push_back(std::make_unique<TagComponent>(tag));
            }
            
            lastStartPos = tagEndPos;
        }
    }
    
    // Process any remaining text
    if (lastStartPos < len) {
        const QStringRef text = source_.midRef(lastStartPos);
        components.push_back(std::make_unique<TextComponent>(text));
    }
    
    return components;
}

QString Liquid::Template::render()
{
    const Context ctx;
    return render(ctx);
}

QString Liquid::Template::render(const Context& ctx)
{
    QString str;
    for (const auto& component : components_) {
        str += component->render(ctx);
    }
    return str;
}



#ifdef TESTS

#include "catch.hpp"

TEST_CASE("Liquid::Template") {
    
    SECTION("NoParse") {
        Liquid::Template t;
        CHECK(t.render() == "");
    }

    SECTION("Empty") {
        Liquid::Template t;
        t.parse("");
        CHECK(t.render() == "");
    }

    SECTION("PlainText") {
        Liquid::Template t;
        t.parse("Hello World!");
        CHECK(t.render() == "Hello World!");
    }

    SECTION("SingleObject") {
        Liquid::Template t;
        t.parse("{{what}}");
        CHECK(t.render() == "");
    }
    
    SECTION("SingleObjectWithText") {
        Liquid::Template t;
        t.parse("Hello {{what}}!");
        CHECK(t.render() == "Hello !");
    }

    SECTION("UnclosedObject") {
        Liquid::Template t;
        CHECK_THROWS_AS(t.parse("{{what"), std::string);
    }
    
    SECTION("BasicObject") {
        Liquid::Template t;
        Liquid::Context::Hash hash;
        hash["what"] = "World";
        Liquid::Context ctx(hash);
        t.parse("Hello {{what}}!");
        CHECK(t.render(Liquid::Context(hash)).toStdString() == "Hello World!");
    }
    
    SECTION("BasicMultipleObjects") {
        Liquid::Template t;
        Liquid::Context::Hash hash;
        hash["fname"] = "Bill";
        hash["lname"] = "Gates";
        Liquid::Context ctx(hash);
        t.parse("Dear {{ fname }} {{ lname }},");
        CHECK(t.render(Liquid::Context(hash)).toStdString() == "Dear Bill Gates,");
    }
    
    SECTION("ObjectKey") {
        Liquid::Template t;
        Liquid::Context::Hash hash;
        Liquid::Context::Hash user;
        user["name"] = "Bob";
        hash["user"] = user;
        Liquid::Context ctx(hash);
        t.parse("Welcome {{user.name}}!");
        CHECK(t.render(Liquid::Context(hash)).toStdString() == "Welcome Bob!");
    }

    SECTION("ObjectBracketKeyStringLiteral") {
        Liquid::Template t;
        Liquid::Context::Hash hash;
        Liquid::Context::Hash user;
        user["name"] = "Bob";
        hash["user"] = user;
        Liquid::Context ctx(hash);
        t.parse("Welcome {{user[\"name\"]}}!");
        CHECK(t.render(Liquid::Context(hash)).toStdString() == "Welcome Bob!");
    }

    SECTION("ObjectBracketKeyObjectNil") {
        Liquid::Template t;
        Liquid::Context::Hash hash;
        Liquid::Context::Hash user;
        user["name"] = "Bob";
        hash["user"] = user;
        Liquid::Context ctx(hash);
        t.parse("Welcome {{user[name]}}!");
        CHECK(t.render(Liquid::Context(hash)).toStdString() == "Welcome !");
    }

    SECTION("ObjectBracketKeyObjectSet") {
        Liquid::Template t;
        Liquid::Context::Hash hash;
        Liquid::Context::Hash user;
        user["name"] = "Bob";
        hash["user"] = user;
        hash["name"] = "name";
        Liquid::Context ctx(hash);
        t.parse("Welcome {{user[name]}}!");
        CHECK(t.render(Liquid::Context(hash)).toStdString() == "Welcome Bob!");
    }

    SECTION("ObjectMultipleBracketKey") {
        Liquid::Template t;
        Liquid::Context::Hash hash;
        Liquid::Context::Hash states;
        Liquid::Context::Hash california;
        Liquid::Context::Hash sacramento;
        Liquid::Context::Array keys;
        keys.push_back("arizona");
        keys.push_back("california");
        sacramento["areacode"] = "916";
        california["capital"] = sacramento;
        states["california"] = california;
        hash["states"] = states;
        hash["keys"] = keys;
        Liquid::Context ctx(hash);
        t.parse("{{states[keys[1]][\"capital\"][\"areacode\"]}} {{keys[0]}} {{keys[9999]}}");
        CHECK(t.render(Liquid::Context(hash)).toStdString() == "916 arizona ");
    }
    
    SECTION("AppendFilter") {
        Liquid::Template t;
        Liquid::Context ctx;
        t.parse("{{ \"hello\" | append: \"world\" }}");
        CHECK(t.render(ctx).toStdString() == "helloworld");
    }

    SECTION("AppendFilterMultiple") {
        Liquid::Template t;
        Liquid::Context ctx;
        t.parse("{{ \"hello\" | append: \"world\", \"planet\", \"galaxy\", \"universe\" }}");
        CHECK(t.render(ctx).toStdString() == "helloworldplanetgalaxyuniverse");
    }
}

#endif
