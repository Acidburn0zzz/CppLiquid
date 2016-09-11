#ifndef LIQUID_NODE_HPP
#define LIQUID_NODE_HPP

#include "variable.hpp"
#include <QStringRef>
#include <memory>

namespace Liquid {
    
    class Context;
    class Tokenizer;
    
    class Node {
    public:
        Node(const Context&) {}
        virtual QString render(Context& context) = 0;
    };

    class TextNode : public Node {
    public:
        TextNode(const Context& context, const QStringRef& text);
        
        virtual QString render(Context&) override;
        
    private:
        const QStringRef text_;
    };
    
    class ObjectNode : public Node {
    public:
        ObjectNode(const Context& context, const Variable& var);

        virtual QString render(Context& context) override;

    private:
        const Variable var_;
    };
    
    class TagNode : public Node {
    public:
        TagNode(const Context& context, const QStringRef& tagName, const QStringRef&)
            : Node(context)
            , tagName_(tagName)
        {}
        virtual QString render(Context& context) override;
        const QStringRef& tagName() {
            return tagName_;
        }
    private:
        const QStringRef tagName_;
    };
    
    using NodePtr = std::shared_ptr<Node>;
}

#endif
