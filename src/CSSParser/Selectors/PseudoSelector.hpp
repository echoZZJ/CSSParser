//
//  PseudoSelector.hpp
//  DDCSSParser
//
//  Created by 1m0nster on 2018/8/7.
//  Copyright © 2018 1m0nster. All rights reserved.
//

#ifndef PseudoSelector_hpp
#define PseudoSelector_hpp

#include <stdio.h>
#include <iostream>
#include "Selector.hpp"

// TODO not finish
namespace future {
    class PseudoSelector: public Selector {
    public:
        enum class ParameterType {
            STRING,
            NUMBER,
            POLYNOMIAL,
            IDENT,
            NONE
        };
        struct Parameter {
            struct polynomial {
                int coefficient;
                int constant;
                int sign;
                polynomial()
                {
                    coefficient = 0;
                    constant = 0;
                    sign = 0;
                }
            }               polynomial;
            std::string     pString;
            int             pNumber;
            ParameterType   type;
            Parameter()
            {
                type = ParameterType::NONE;
                pNumber = 0;
                pString = "";
            }
        };
    public:
        PseudoSelector(const std::string& data)
        {
            m_selectorType = Selector::PseudoSelector;
            m_data = data;
            m_parameter = NULL;
        }
        
        ~PseudoSelector();
        
        bool isBaseSelector();
        
        int weight();
        
        void setParameter(Parameter *);
        
        std::string getPseudoData();
        
        Parameter* getParameter();
    private:
        std::string m_data;
        Parameter* m_parameter;
    };
}

#endif /* PseudoSelector_hpp */
