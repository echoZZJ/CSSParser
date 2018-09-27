//
//  PseudoSelectorRefChiefJudge.cpp
//  DDCSSParser
//
//  Created by 1m0nster on 2018/9/12.
//  Copyright © 2018 1m0nster. All rights reserved.
//

#include "PseudoSelectorRefChiefJudge.hpp"
#include <string>
#include "StringUtil.h"

const std::string P_LINK = "link";
const std::string P_VISITED = "visited";
const std::string P_HOVER = "hover";
const std::string P_ACTIVE = "active";
const std::string P_FOCUS = "focus";
const std::string P_ENABLE = "enable";
const std::string P_DISABLE = "disable";
const std::string P_CHECKED = "checked";
const std::string P_INDETERMINATE = "indeterminate";
const std::string P_ROOT = "root";
const std::string P_NTHCHILD = "nth-child";
const std::string P_NTHLASTCHILD = "nth-last-child";
const std::string P_NTHOFTYPE = "nth-of-type";
const std::string P_NTHLASTOFTYPE = "nth-last-of-type";
const std::string P_FIRSTCHILD = "first-child";
const std::string P_LASTCHILD = "last-child";
const std::string P_FIRSTOFTYPE = "first-of-type";
const std::string P_LASTOFTYPE = "last-of-type";
const std::string P_ONLYCHILD = "only-child";
const std::string P_ONLYOFTYPE = "only-of-type";
const std::string P_EMPTY = "empty";
const std::string P_TARGET = "target";
const std::string P_LANG = "lang";

namespace future {
    static std::map<std::string, bool> S_dynamicPsudoClassCacheMap;
    struct TraverseUtilStruct0 {
        GumboNode* selfNode;
        int idx;
        TraverseUtilStruct0()
        {
            selfNode = NULL;
            idx = 0;
        }
    };
    struct TraverseUtilStruct1 {
        GumboNode* selfNode;
        int idx;
        int totalCount;
        TraverseUtilStruct1()
        {
            selfNode = NULL;
            idx = 0;
            totalCount = 0;
        }
    };
    void nodeIndexCounter(GumboNode* node, bool& stop, void *userData);
    void nodeFullIndexCounter(GumboNode* node, bool& stop, void*userData);
    
    bool PseudoSelectorRefChiefJudge::nodeAdaptToPseudo(GumboNode *node, future::PseudoSelector *selector)
    {
        std::string ruleData = selector->getPseudoData();
        if (ruleData.empty()) {
            return false;
        }
        if (getDynamicPseudoClassMap()[ruleData]) {
            return true;
        }
        if (ruleData == P_ROOT) {
            return !node->parent;
        }
        if (ruleData == P_NTHCHILD) {
            PseudoSelector::Parameter* p = selector->getParameter();
            if (!p) {return false;}
            return indexAdaptToPoly(p, indexOfSiblings(node));
        }
        return true;
    }
    
    int PseudoSelectorRefChiefJudge::indexOfSiblings(GumboNode* node)
    {
        GumboNode* parent = node->parent;
        if (!parent) {
            return 0;
        }
        TraverseUtilStruct0 baseInfo;
        traverseElementNodeSiblings(node, nodeIndexCounter, &baseInfo);
        return baseInfo.idx;
    }
    
    void PseudoSelectorRefChiefJudge::traverseElementNodeSiblings(GumboNode *node, traverseAction ac, void* userData)
    {
        GumboNode* parent = node->parent;
        if (!parent) {
            return ;
        }
        GumboVector children = parent->v.document.children;
        bool _stop;
        bool &stop = _stop;
        for (int i = 0; i < children.length; i ++) {
            if (((GumboNode *)children.data[i])->type == GUMBO_NODE_ELEMENT) {
                ac((GumboNode *)children.data[i], stop, userData);
                if (stop) {
                    break;
                }
            }
        }
    }
    
    bool PseudoSelectorRefChiefJudge::indexAdaptToPoly(PseudoSelector::Parameter *parameter, int idx)
    {
        bool adapt = false;
        if (!parameter) {
            return false;
        }
        switch (parameter->type) {
            case PseudoSelector::ParameterType::NUMBER: {
                adapt = idx == parameter->pNumber;
                break;
            }
            case PseudoSelector::ParameterType::STRING: {
                adapt = false;
                break;
            }
            case PseudoSelector::ParameterType::POLYNOMIAL: {
                adapt = formulaHaveIntergerSolution(parameter, idx);
                break;
            }
            case PseudoSelector::ParameterType::IDENT: {
                PseudoSelector::Parameter p;
                p.polynomial.coefficient = 2;
                if (StringUtil::tolower(parameter->pString) == "even") {
                    p.polynomial.constant = 0;
                    adapt = formulaHaveIntergerSolution(&p, idx);
                } else if (StringUtil::tolower(parameter->pString) == "odd") {
                    p.polynomial.constant = 1;
                    adapt = formulaHaveIntergerSolution(&p, idx);
                } else {
                    adapt = false;
                }
                break;
            }
                
            default:
                break;
        }
        return adapt;
    }
    
    bool PseudoSelectorRefChiefJudge::formulaHaveIntergerSolution(PseudoSelector::Parameter *parameter, int idx)
    {
        if (idx < parameter->polynomial.constant) {
            return false;
        }
        return !((idx - parameter->polynomial.sign * parameter->polynomial.constant) % parameter->polynomial.coefficient);
    }
    
    std::map<std::string, bool>& PseudoSelectorRefChiefJudge::getDynamicPseudoClassMap()
    {
        if (!S_dynamicPsudoClassCacheMap.empty()) {
            return S_dynamicPsudoClassCacheMap;
        }
        S_dynamicPsudoClassCacheMap[P_LINK] = true;
        S_dynamicPsudoClassCacheMap[P_VISITED] = true;
        S_dynamicPsudoClassCacheMap[P_HOVER] = true;
        S_dynamicPsudoClassCacheMap[P_ACTIVE] = true;
        S_dynamicPsudoClassCacheMap[P_FOCUS] = true;
        S_dynamicPsudoClassCacheMap[P_ENABLE] = true;
        S_dynamicPsudoClassCacheMap[P_DISABLE] = true;
        S_dynamicPsudoClassCacheMap[P_CHECKED] = true;
        S_dynamicPsudoClassCacheMap[P_INDETERMINATE] = true;
        return S_dynamicPsudoClassCacheMap;
    }
    
    void nodeIndexCounter(GumboNode* node, bool& stop, void *userData)
    {
        TraverseUtilStruct0*baseInfo = reinterpret_cast<TraverseUtilStruct0 *>(userData);
        if (!baseInfo) {
            return;
        }
        baseInfo->idx += 1;
        if (node == baseInfo->selfNode) {
            stop = true;
        }
    }
    
    void nodeFullIndexCounter(GumboNode* node, bool& stop, void*userData)
    {
        TraverseUtilStruct1*baseInfo = reinterpret_cast<TraverseUtilStruct1 *>(userData);
        if (!baseInfo) {
            return;
        }
        baseInfo->totalCount += 1;
        if (node == baseInfo->selfNode) {
            baseInfo->idx = baseInfo->totalCount;
        }
    }
}
