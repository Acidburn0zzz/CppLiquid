#ifndef LIQUID_BLOCK_HPP
#define LIQUID_BLOCK_HPP

#include "node.hpp"
#include "blockbody.hpp"

namespace Liquid {
    
    class BlockTag : public TagNode {
    public:
        BlockTag(const Context& context, const QStringRef& tagName, const QStringRef& markup);
        
        virtual void parse(const Context& context, Tokenizer& tokenizer);
        
        bool parseBody(const Context& context, BlockBody* body, Tokenizer& tokenizer);
        
        virtual QString render(Context& context) override;
        
    protected:
        virtual void handleUnknownTag(const QStringRef& tagName, const QStringRef& markup, Tokenizer& tokenizer);

        BlockBody body_;

    private:
        QStringRef tagName_;
    };
}

#endif
