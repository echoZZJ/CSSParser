//
//  CSSParser.cpp
//  DDCSSParser
//
//  Created by 1m0nster on 2018/8/7.
//  Copyright © 2018 1m0nster. All rights reserved.
//

#include "CSSParser.hpp"
#include "CSSLex.hpp"
#include <string.h>
#include <assert.h>
#include "ContainerUtil.hpp"
#include "StringUtil.h"

namespace future {
    
    CSSParser::ASTNode* TreeTranverseCreateExpressionAction(std::stack<
                                                            CSSParser::ASTNode *>* stack);
    static void cleanASTTree(CSSParser::ASTNode *);
#define PopOperand(left,right) ASTNode*left = operandStack.top();\
operandStack.pop();\
ASTNode*right = operandStack.top();\
operandStack.pop()
#define PopOperator(node) ASTNode* node = operatorStack.top();\
operatorStack.pop();
    CSSParser::CSSParser() {
        m_lexer = new Lex;
    }
    
    CSSParser::~CSSParser() {
        clean();
    }
    
    template<class T>
    static void eraseStack(std::stack<T>& stack) {
        std::stack<T> st;
        stack.swap(st);
    }
    
    void CSSParser::prepareByFile(const std::string &cssPath)
    {
        m_hostCssFile = cssPath;
        m_selectors.clear();
        m_lexer->SetBufferSource(cssPath);
    }
    
    void CSSParser::prepareByString(const std::string &cssString)
    {
        m_hostCssFile.clear();
        m_selectors.clear();
        m_lexer->SetBufferString(cssString);
    }
    
    bool CSSParser::parseByFile(const std::string &cssPath) {
        if (cssPath.empty()) {
            return false;
        }
        prepareByFile(cssPath);
        return parse();
    }
    
    bool CSSParser::parseByString(const std::string &cssString)
    {
        if (cssString.empty()) {
            return false;
        }
        prepareByString(cssString);
        return parse();
    }
    
    bool CSSParser::parse()
    {
        std::stack<Selector *> syntaxStack;
        Lex::CSSToken *token = m_lexer->GetToken();
        m_status = START;
        while (token->type != END && token->type != ERROR) {
            switch (m_status) {
                case START: {
                    if (token->type == WS) {
                        m_status = START;
                    } else if (startSelector(token->type)) {
                        Selector* selector = getSelector(token);
                        if (!selector) {
                            return false;
                        }
                        syntaxStack.push(selector);
                        m_status = INSELECTOR;
                    } else if (token->type == ATKEYWORD) {
                        m_status = INATKEYWORD;
                        KeywordItem* keyword = new KeywordItem(token->data);
                        m_keywords.push_back(keyword);
                    }
                    break;
                }
                case INSELECTOR: {
                    if (token->type == WS) {
                        pushSign(syntaxStack, SignSelector::NormalInherit);
                    } else if (token->type == GREATER) {
                        pushSign(syntaxStack, SignSelector::Greater);
                    } else if (token->type == PLUS) {
                        pushSign(syntaxStack, SignSelector::Plus);
                    } else if (token->type == TIDLE) {
                        pushSign(syntaxStack, SignSelector::Tidle);
                    } else if (token->type == COMMA) {
                        pushSign(syntaxStack, SignSelector::Comma);
                    } else if (token->type == BLOCKSTART) {
                        m_status = STARTBLOCK;
                    } else {
                        Selector* s = getSelector(token);
                        if (s) {
                            if (!topHaveSign(syntaxStack) && syntaxStack.size()) {
                                pushSign(syntaxStack, SignSelector::Concat);
                            }
                            syntaxStack.push(s);
                        } else {
                            return false;
                        }
                    }
                    break;
                }
                case INATKEYWORD: {
                    if (token->type == BLOCKSTART) {
                        Lex::CSSToken* t = m_lexer->GetToken();
                        if (t->type == BLOCKEND) {
                            m_status = START;
                            KeywordItem* keyword = *--m_keywords.end();
                            keyword->setData(t->data);
                        }
                    }
                    break;
                }
                case STARTBLOCK: {
                    // create selector ATS
                    if (token->type == BLOCKEND) {
                        if (topHaveSign(syntaxStack)) {
                            syntaxStack.pop();
                        }
                        std::list<ASTNode *> astContainer = createATS(syntaxStack);
                        eraseStack(syntaxStack);
                        std::list<ASTNode *>::iterator it = astContainer.begin();
                        std::list<ASTNode *>::iterator end = astContainer.end();
                        bool isGroupSelector = astContainer.size() > 1;
                        GroupSelector* group = isGroupSelector ? new GroupSelector : 0;
                        while (it != end) {
                            std::stack<CSSParser::ASTNode *> *result = new std::stack<
                            CSSParser::ASTNode *>;
                            MLRtranverseAST(*it, TreeTranverseCreateExpressionAction,
                                            result);
                            ASTNode* node = result->top();
                            if (isGroupSelector) {
                                group->addSelector(node->head);
                            } else {
                                node->head->setRuleData(token->data);
                                node->head->setHostCSSFilePath(m_hostCssFile);
                                m_selectors.push_front(node->head);
                            }
                            LRMtranverseAST(*it++, cleanASTTree);
                        }
                        if (isGroupSelector) {
                            m_selectors.push_back(group);
                            group->setRuleData(token->data);
                            group->setHostCSSFilePath(m_hostCssFile);
                        }
                        m_status = START;
                    } else {
                        return false;
                    }
                    
                    break;
                }
                default:
                    break;
            }
            token = m_lexer->GetToken();
        }
        return true;
    }
    
    Selector* CSSParser::getSelector(Lex::CSSToken* token) {
        Selector* selector = NULL;
        switch (token->type) {
            case STAR: {
                selector = new UniversalSelector();
                break;
            }
            case DOT: {
                Lex::CSSToken* t = m_lexer->GetToken();
                if (t->type == IDENT) {
                    if (!t->data.empty()) {
                        ClassSelector* s = new ClassSelector(t->data);
                        selector = s;
                    }
                }
                break;
            }
            case HASH: {
                if (!token->data.empty()) {
                    std::string& id = token->data;
                    id = id.substr(1, id.length() - 1);
                    IdSelector* s = new IdSelector(id);
                    selector = s;
                }
                break;
            }
            case LEFTSQUREBRACKET: {
                Lex::CSSToken* t = m_lexer->GetToken();
                enum {
                    _start = 0,
                    _key,
                    _sign,
                    _value,
                    _end,
                    _error
                };
                std::string key;
                std::string value;
                char _status = _start;
                AttributeSelector::attributeFilterRule rule = AttributeSelector::NoRule;
                while (t->type != END || t->type != ERROR) {
                    switch (_status) {
                        case _start: {
                            if (t->type == WS) {
                                _status = _start;
                            } else if (t->type == IDENT) {
                                _status = _key;
                                if (!t->data.empty()) {
                                    key = t->data;
                                }
                            } else {
                                _status = _error;
                            }
                            break;
                        }
                        case _key: {
                            _status = _sign;
                            if (t->type == WS) {
                                _status = _key;
                            } else if (t->type == INCLUDES) {
                                // ~=
                                rule = AttributeSelector::Include;
                            } else if (t->type == DASHMATCH) {
                                // |=
                                rule = AttributeSelector::DashMatch;
                            } else if (t->type == PREFIXMATCH) {
                                // ^=
                                rule = AttributeSelector::Prefix;
                            } else if (t->type == SUFFIXMATCH) {
                                // $=
                                rule = AttributeSelector::Suffix;
                            } else if (t->type == SUBSTRINGMATCH) {
                                // *=
                                rule = AttributeSelector::Substring;
                            } else if (t->type == RIGHTSQUREBRACKET) {
                                // ]
                                _status = _end;
                            } else if (t->type == EQUAL) {
                                // =
                                rule = AttributeSelector::Equal;
                            } else {
                                _status = _error;
                            }
                            break;
                        }
                        case _sign: {
                            if (t->type == WS) {
                                _status = _sign;
                            } else if (t->type == IDENT || t->type == STRING) {
                                _status = _value;
                                if (t->data.empty()) {
                                    break;
                                }
                                value = t->data;
                                if (t->type == STRING) {
                                    value = value.substr(1, value.length() - 2);
                                }
                            } else {
                                _status = _error;
                            }
                            break;
                        }
                        case _value: {
                            if (t->type == WS) {
                                _status = _value;
                            } else if (t->type == RIGHTSQUREBRACKET) {
                                _status = _end;
                            } else {
                                _status = _error;
                            }
                            break;
                        }
                        default: {
                            _status = _error;
                        }
                            break;
                    }
                    if (_status == _error) {
                        break;
                    } else if (_status == _end) {
                        // generate selector
                        AttributeSelector* s = new AttributeSelector(key, value, rule);
                        selector = s;
                        break;
                    }
                    t = m_lexer->GetToken();
                }
                break;
            }
            case IDENT: {
                TypeSelector* s = new TypeSelector(token->data);
                selector = s;
                break;
            }
            case COLON: {
                Lex::CSSToken* t = m_lexer->GetToken();
                if (t->type == IDENT) {
                    PseudoSelector* s = new PseudoSelector(t->data);
                    selector = s;
                } else if (t->type == FUNCTION) {
                    PseudoSelector* s = new PseudoSelector(t->data);
                    PseudoSelector::Parameter* parameter = getFunctionParamenter();
                    if (parameter) {
                        s->setParameter(parameter);
                    }
                    selector = s;
                } else {
                    return selector;
                }
                break;
            }
                
            default:
                break;
        }
        return selector;
    }
    
    PseudoSelector::Parameter* CSSParser::getFunctionParamenter()
    {
#define CleanRetAndStopLoop delete parameter; parameter = NULL; endLoop = true
        enum {_start, _inNumber, _inString, _inIdent, _inPolynomialLeft, _inPolynomialRight, _number, _polynomial, _error};
        PseudoSelector::Parameter* parameter = new PseudoSelector::Parameter;
        char state = _start;
        bool endLoop = false;
        Lex::CSSToken* token = m_lexer->GetToken();
        while (1) {
            switch (state) {
                case _start: {
                    if (token->type == NUMBER) {
                        parameter->type = PseudoSelector::ParameterType::NUMBER;
                        parameter->pNumber = StringUtil::str2int(token->data);
                        state = _inNumber;
                    } else if (token->type == IDENT) {
                        parameter->type = PseudoSelector::ParameterType::IDENT;
                        state = _inIdent;
                        parameter->pString = token->data;
                    } else if (token->type == STRING) {
                        parameter->type = PseudoSelector::ParameterType::STRING;
                        state = _inString;
                        parameter->pString = token->data;
                    } else if (token->type == WS) {
                        state = _start;
                        break;
                    } else if (token->type == RIGHTBRACKET) {
                        CleanRetAndStopLoop;
                    } else {
                        CleanRetAndStopLoop;
                        state = _error;
                    }
                    break;
                }
                case _inString:
                case _inIdent: {
                    if (token->type == WS) {
                        break;
                    } else if (token->type == RIGHTBRACKET) {
                        endLoop = true;
                    } else {
                        state = _error;
                        CleanRetAndStopLoop;
                    }
                    break;
                }
                case _inNumber: {
                    if (token->type == WS) {
                        state = _number;
                    } else if (token->type == IDENT) {
                        state = _inPolynomialLeft;
                        parameter->type = PseudoSelector::ParameterType::POLYNOMIAL;
                        parameter->polynomial.coefficient = parameter->pNumber;
                    } else if (token->type == RIGHTBRACKET) {
                        endLoop = true;
                        state = _number;
                    } else {
                        CleanRetAndStopLoop;
                        state = _error;
                    }
                    break;
                }
                case _inPolynomialLeft: {
                    if (token->type == WS) {
                        state = _inPolynomialLeft;
                    } else if (token->type == PLUS) {
                        parameter->polynomial.sign = 1;
                        state = _inPolynomialRight;
                    } else if (token->type == MINUS) {
                        parameter->polynomial.sign = -1;
                        state = _inPolynomialRight;
                    } else {
                        CleanRetAndStopLoop;
                        state = _error;
                    }
                    break;
                }
                case _inPolynomialRight: {
                    if (token->type == WS) {
                        break;
                    } else if (token->type == NUMBER) {
                        parameter->polynomial.constant = StringUtil::str2int(token->data);
                        state = _polynomial;
                    } else if (token->type == RIGHTBRACKET) {
                        endLoop = true;
                        state = _polynomial;
                    } else {
                        CleanRetAndStopLoop;
                        state = _error;
                    }
                    break;
                }
                case _number:
                case _polynomial: {
                    if (token->type == WS) {
                        break;
                    } else if (token->type == RIGHTBRACKET) {
                        endLoop = true;
                    } else {
                        CleanRetAndStopLoop;
                        state = _error;
                    }
                    break;
                }
                case _error: {
                    return NULL;
                }
                default: {
                    CleanRetAndStopLoop;
                    state = _error;
                    break;
                }
            }
            if (endLoop) {break;}
            token = m_lexer->GetToken();
        }
        return parameter;
    }
    
    std::list<CSSParser::ASTNode *> CSSParser::createATS(std::stack<Selector *>&syntax) {
        std::stack<ASTNode *> operatorStack;
        std::stack<ASTNode *> operandStack;
        std::list<ASTNode *> atsCollection;
        while (topHaveSign(syntax)) {
            syntax.pop();
        }
        if (!syntax.size()) {
            return atsCollection;
        }
        while (syntax.size()) {
            Selector* s = syntax.top();
            syntax.pop();
            SignSelector* newOperator = dynamic_cast<SignSelector *> (s);
            if (!newOperator) {
                pushOperatedElement(operandStack, s);
                continue;
            }
            if (!operatorStack.size()) {
                if (newOperator->getSignType() == SignSelector::Comma) {
                    atsCollection.push_back(operandStack.top());
                    operandStack.pop();
                    continue;
                }
                pushOperatedElement(operatorStack, s);
                continue;
            }
            SignSelector* oldOperator =
            dynamic_cast<SignSelector *> (operatorStack.top()->head);
            if (newOperator->getSignType() == SignSelector::Comma) {
                // close current ATS and put it in the list
                buildReversePolishNotation(operatorStack, operandStack);
                ASTNode* root = operandStack.top();
                operandStack.pop();
                atsCollection.push_back(root);
                continue;
            }
            if (newOperator->getSignType() == SignSelector::Concat
                && oldOperator->getSignType() != SignSelector::Concat) {
                pushOperatedElement(operatorStack, s);
                continue;
            }
            // create ATS
            {
                buildReversePolishNotation(operatorStack, operandStack);
                pushOperatedElement(operatorStack, s);
            }
        }
        if (operandStack.size() && operatorStack.size()) {
            buildReversePolishNotation(operatorStack, operandStack);
        }
        atsCollection.push_back(operandStack.top());
        return atsCollection;
    }
    
    void CSSParser::buildReversePolishNotation(
                                               std::stack<ASTNode *> &operatorStack,
                                               std::stack<ASTNode *> &operandStack) {
        while (operatorStack.size()) {
            PopOperand(leftNode, rightNode);
            PopOperator(head);
            initialASTNode(head, 0, leftNode, rightNode);
            operandStack.push(head);
        }
    }
    
    void CSSParser::LMRtranverseAST(future::CSSParser::ASTNode *root,
                                    future::CSSParser::treeTranverseAction action) {
        if (!root) {
            return;
        }
        if (root->left) {
            LMRtranverseAST(root->left, action);
        }
        action(root);
        if (root->right) {
            LMRtranverseAST(root->right, action);
        }
    }
    
    void CSSParser::RMLtranverseAST(future::CSSParser::ASTNode *root,
                                    treeTranverseAction action) {
        if (!root) {
            return;
        }
        if (root->right) {
            RMLtranverseAST(root->right, action);
        }
        action(root);
        if (root->left) {
            RMLtranverseAST(root->left, action);
        }
    }
    
    void CSSParser::LRMtranverseAST(future::CSSParser::ASTNode *root,
                                    future::CSSParser::treeTranverseAction action) {
        if (!root) {
            return;
        }
        if (root->left) {
            LRMtranverseAST(root->left, action);
        }
        if (root->right) {
            LRMtranverseAST(root->right, action);
        }
        action(root);
    }
    
    void CSSParser::MLRtranverseAST(ASTNode *root,
                                    treeTranverseWithUserDataAction action, void *userData) {
        if (!root) {
            return;
        }
        std::stack<CSSParser::ASTNode *> *oldStack = (std::stack<
                                                      CSSParser::ASTNode *> *) userData;
        std::stack<CSSParser::ASTNode *> *newStack = new std::stack<
        CSSParser::ASTNode *>;
        newStack->push(root);
        if (root->left) {
            MLRtranverseAST(root->left, action, newStack);
        }
        if (root->right) {
            MLRtranverseAST(root->right, action, newStack);
        }
        size_t size = newStack->size();
        if (size == 3) {
            ASTNode* n = TreeTranverseCreateExpressionAction(newStack);
            oldStack->push(n);
        } else if (size == 1) {
            oldStack->push(newStack->top());
        }
        delete newStack;
    }
    
    CSSParser::ASTNode* TreeTranverseCreateExpressionAction(std::stack<
                                                            CSSParser::ASTNode *>* stack) {
        typedef CSSParser::ASTNode ASTNode;
        if (stack->size() != 3) {
            return 0;
        }
        Selector* _right = stack->top()->head;
        stack->pop();
        Selector* _left = stack->top()->head;
        stack->pop();
        SignSelector* _operator = dynamic_cast<SignSelector *> (stack->top()->head);
        stack->pop();
        if (!_operator) {
            assert(0);
        }
        ASTNode* node = 0;
        Selector* selector = 0;
        switch (_operator->getSignType()) {
            case SignSelector::Plus: {
                // +
                CombineSelector* s = new CombineSelector;
                s->initialInstanceSiblingList(_left, _right);
                selector = s;
                break;
            }
            case SignSelector::Tidle: {
                // ~
                CombineSelector* s = new CombineSelector;
                s->initialNormalSiblingList(_left, _right);
                selector = s;
                break;
            }
            case SignSelector::NormalInherit: {
                // ' '
                CombineSelector* s = new CombineSelector;
                s->initialNormalInhericalList(_left, _right);
                selector = s;
                break;
            }
            case SignSelector::Concat: {
                SequenceSelector* s = new SequenceSelector;
                s->appendSelector(_left);
                s->appendSelector(_right);
                selector = s;
                break;
            }
            case SignSelector::Greater: {
                // >
                CombineSelector* s = new CombineSelector;
                s->initialInstanceInhericalList(_left, _right);
                selector = s;
                break;
            }
            default:
                break;
        }
        node = new ASTNode;
        node->head = selector;
        return node;
    }
    
    bool CSSParser::startSelector(CSSTokenType type) {
        return (type == IDENT || type == DOT || type == HASH || type
                == LEFTSQUREBRACKET || type == STAR || type == COLON);
    }
    
    bool CSSParser::tokenHasInfo(CSSTokenType type) {
        return (type == IDENT || type == HASH || type == STAR);
    }
    
    bool CSSParser::topHaveSign(std::stack<Selector *>& stack) {
        if (stack.empty()) {
            return false;
        }
        Selector* topSelector = stack.top();
        return topSelector->getType() == Selector::SignSelector;
    }
    
    std::list<Selector *> CSSParser::getSelectors() {
        return m_selectors;
    }
    
    std::list<KeywordItem *> CSSParser::getKeywords() {
        return m_keywords;
    }
    
    void CSSParser::pushSign(std::stack<Selector *> &stack,
                             SignSelector::SignType type) {
        if (!stack.size()) {
            return;
        }
        if (!topHaveSign(stack)) {
            SignSelector* s = new SignSelector(type);
            stack.push(s);
        }
    }
    
    void CSSParser::pushOperatedElement(std::stack<ASTNode *> &stack,
                                        future::Selector *head) {
        ASTNode* n = new ASTNode;
        initialASTNode(n, head, 0, 0);
        stack.push(n);
    }
    
    void CSSParser::initialASTNode(ASTNode *target, Selector* head, ASTNode* left,
                                   ASTNode* right) {
        if (head) {
            target->head = head;
        }
        if (left) {
            target->left = left;
        }
        if (right) {
            target->right = right;
        }
    }
    
    void CSSParser::clean() {
        CleanContainer(m_tokenStack);
        CleanContainer(m_selectors);
        CleanContainer(m_keywords);
        if (m_lexer) {
            m_lexer->CleanResource();
            delete m_lexer;
            m_lexer = NULL;
        }
    }
    
    static void cleanASTTree(CSSParser::ASTNode *node) {
        node->left = 0;
        node->right = 0;
        node->head = 0;
        delete node;
    }
}
