#include "dparse.h"
//#include "FiniteAutomata.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <stack>
#include <algorithm>

extern "C"{
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}
using namespace std;

ParserRunner::ParserRunner(LexerRunner* lexer):_lexer(lexer){
}

void ParserRunner::setParser(Parser* parser){
    _parser = parser;
    lua_pushlightuserdata(_parser->L, this);
    lua_pushcclosure(_parser->L, &ParserRunner::getstackcontent, 1);
    lua_setglobal(_parser->L, "parser_getstackcontent");
    lua_pushlightuserdata(_parser->L, this);
    lua_pushcclosure(_parser->L, &ParserRunner::getcurrenttokenname, 1);
    lua_setglobal(_parser->L, "parser_getcurrenttokenname");
}

int ParserRunner::getstackcontent(lua_State* L){
    ParserRunner *p = (ParserRunner*)lua_touserdata(L, lua_upvalueindex(1));
    int i = lua_tointeger(L, 1);
    lua_pushinteger(L, p->_stk[p->_stk.size() + i].val);
    return 1;
}


int ParserRunner::getcurrenttokenname(lua_State* L){
    ParserRunner *p = (ParserRunner*)lua_touserdata(L, lua_upvalueindex(1));
    lua_pushstring(L, p->_lexer->peekPrevToken().token);
    return 1;
}

bool ParserRunner::parse(){
    //stack<int> stk;
    while(!_stk.empty())
        _stk.pop_back();


    Token token;
    _stk.push_back(State(0, -1));
    bool flag = true;
    while(flag){
        token = _lexer->getNextToken();
        if(token.token == NULL){
            token.tokenType = -1;
            //flag = false;
        }

        ActionKey ackey = {_stk.back().state, token.tokenType};
        GotoKey gtkey;
        map<ActionKey, ActionValue>::iterator ret = _parser->actionTable.find(ackey);
        map<GotoKey, int>::iterator gotoRet;
        if(ret == _parser->actionTable.end())
            return false;
        int temp;
        int luaRet;
        switch(ret->second.action){
        case Shift:
            _stk.push_back(State(ret->second.extra, -1));
            break;
        case Reduce:
            _lexer->putBack();
            //CallFunction
            cout<<_parser->actionFunctions[ret->second.extra3]<<endl;
            lua_getglobal(_parser->L, _parser->actionFunctions[ret->second.extra3].c_str());
            if(lua_pcall(_parser->L, 0, 1, 0)){
                cout<<"Lua error encountered while calling function: "<<_parser->actionFunctions[ret->second.extra3]
                   << "\nError info:" << lua_tostring(_parser->L, -1) << endl;
                lua_pop(_parser->L, 1);
            }

            luaRet = lua_tonumber(_parser->L, -1);
            if(luaRet == -1)
                return false;

            temp = ret->second.extra;
            while(temp--)
                _stk.pop_back();
            gtkey.state = _stk.back().state;
            gtkey.next_nonterminal_id = ret->second.extra2;
            gotoRet = _parser->gotoTable.find(gtkey);
            if(gotoRet == _parser->gotoTable.end())
                return false;

            _stk.push_back(State(gotoRet->second, luaRet));
            //cout<<"asdf"<<lua_tonumber(_parser->L, -1)<<endl;
            lua_pop(_parser->L, -1);
            break;
        case Acc:
            //_lexer->putBack();
            lua_getglobal(_parser->L, "parse_complete");
            if(lua_pcall(_parser->L, 0, 0, 0)){
                cout<<"Lua error encountered while calling function: "<<"parse_complete"
                   << "\nError info:" << lua_tostring(_parser->L, -1) << endl;
                lua_pop(_parser->L, 1);
            }
            return true;
            break;
        default:
            return false;
        }
    }

    return false;
}

Parser* ParserBuilder::buildFromFile(const char* filename){
    _parser.actionTable.clear();
    _parser.gotoTable.clear();
    _parser.actionFunctions.clear();

    if(!parsefile(filename))
        cout<<"Error while parsing file Error No:"<<_errno<<endl;

    calcItemsSetAndGotoTable();

    calcActionAndGotoTable();

    return &_parser;
}

void ParserBuilder::calcActionAndGotoTable(){
    for(int i = 0; i < _lrItemsId.size(); i++){
        for(LRItemsType::iterator it = _lrItemsId[i]->begin(); it != _lrItemsId[i]->end(); it++){
            if(it->position + 1 < _rules[it->rule_id].size()){
                Symbol sym = _rules[it->rule_id][it->position + 1];
                PrivateGotoKey key = {i, sym};
                map<PrivateGotoKey, int>::iterator gotoState;
                if(sym.is_terminal && (gotoState = _gotoTable.find(key)) != _gotoTable.end()){
                    ActionKey ak = {i, sym.index};
                    ActionValue av = {Shift, gotoState->second, 0, 0};
                    _parser.actionTable.insert(make_pair(ak, av));
                }
            }else if(it->position + 1 == _rules[it->rule_id].size()){
                Symbol tempsym = {false, 0};
                if(_rules[it->rule_id][0] != tempsym){
                    ActionKey ak = {i, it->nextchar_id};
                    ActionValue av = {Reduce,
                                      int(_rules[it->rule_id].size() - 1),
                                      _rules[it->rule_id][0].index,
                                      _rulesFunction[it->rule_id - 1]
                                     };
                    _parser.actionTable.insert(make_pair(ak, av));
                }else if(it->nextchar_id == -1){
                    ActionKey ak = {i, -1};
                    ActionValue av = {Acc, 0, 0, 0};
                    _parser.actionTable.insert(make_pair(ak, av));
                }
            }
        }
    }

    for(map<PrivateGotoKey, int>::iterator it = _gotoTable.begin(); it != _gotoTable.end(); it++){
        if(!it->first.next_symbol.is_terminal){
            GotoKey key = {it->first.id, it->first.next_symbol.index};
            _parser.gotoTable.insert(make_pair(key, it->second));
        }
    }
}

int ParserBuilder::findItemsSetId(LRItemsSetType::iterator it){
    vector<const LRItemsType*>::iterator ret = find(_lrItemsId.begin(), _lrItemsId.end(), &(*it));
    if(ret == _lrItemsId.end())
        return -1;
    return ret - _lrItemsId.begin();
}

void ParserBuilder::calcItemsSetAndGotoTable(){
    LRItem item0 = {0, 0, -1};
    LRItemsType s;
    s.insert(item0);

    //for(map<string, int>::iterator iter = _terminals.begin(); iter != _terminals.end(); iter++){
      //  item0.nextchar_id = iter->second;
       // s.insert(item0);
    //}

    LRItemsSetType::iterator it = enclosure(s);
    stack<LRItemsSetType::iterator> stk;

    stk.push(it);

    while(!stk.empty()){
        LRItemsSetType::iterator curIter = stk.top();
        stk.pop();
        for(map<string, int>::iterator iter = _terminals.begin(); iter != _terminals.end(); iter++){
            Symbol sym = {true, iter->second};
            unsigned int size = _lrItemsSet.size();
            LRItemsSetType::iterator ret = gotonext(curIter, sym);
            if(size != _lrItemsSet.size())
                stk.push(ret);
        }

        for(unsigned int i = 0; i < _nonTerminals.size(); i++){
            Symbol sym = {false, int(i)};
            unsigned int size = _lrItemsSet.size();
            LRItemsSetType::iterator ret = gotonext(curIter, sym);
            if(size != _lrItemsSet.size())
                stk.push(ret);
        }
    }

}

ParserBuilder::LRItemsSetType::iterator ParserBuilder::gotonext(LRItemsSetType::iterator it, const Symbol& sym){
    LRItemsType items;
    for(LRItemsType::iterator itt = it->begin(); itt != it->end(); itt++){
        LRItem item = *itt;
        if(item.position + 1 >= _rules[item.rule_id].size())
            continue;
        if(_rules[item.rule_id][item.position + 1] != sym)
            continue;
        item.position++;
        items.insert(item);
    }
    LRItemsSetType::iterator i = enclosure(items);
    if(i != _lrItemsSet.end()){
        PrivateGotoKey key = {findItemsSetId(it), sym};
        _gotoTable.insert(make_pair(key, findItemsSetId(i)));
    }

    return i;
}

set<ParserBuilder::Symbol> ParserBuilder::first(const ParserBuilder::Symbol& sym){
    vector<int> temp;
    return first(sym, temp);
}

set<ParserBuilder::Symbol> ParserBuilder::first(const ParserBuilder::Symbol& sym, vector<int>& visited){
    set<Symbol> s, ret;
    if(sym.is_terminal){
        s.insert(sym);
        return s;
    }

    for(unsigned int i = 0; i < _rules.size(); i++){
        if(_rules[i][0] != sym)
            continue;
        if(find(visited.begin(), visited.end(), i) != visited.end())
            continue;

        visited.push_back(i);

        ret = first(_rules[i][1], visited);
        set_union(s.begin(), s.end(), ret.begin(), ret.end(), inserter(s, s.begin()));
    }
    return s;
}

ParserBuilder::LRItemsSetType::iterator ParserBuilder::enclosure(const ParserBuilder::LRItemsType& itemset){
    LRItemsType enclosure = itemset;
    stack<LRItemsType::iterator> s;

    for(LRItemsType::iterator it = enclosure.begin(); it != enclosure.end(); it++){
        s.push(it);
    }


    while(!s.empty()){
        LRItemsType::iterator itt = s.top();
        s.pop();
        //For each item in I

        if(itt->position + 1 >= _rules[itt->rule_id].size())
            continue;

        Symbol sym = _rules[itt->rule_id][itt->position + 1];
        if(sym.is_terminal)
            continue;
        for(unsigned int i = 0; i < _rules.size(); i++){
            if(_rules[i][0] != sym)
                continue;
            //For each rule

            set<Symbol> syms;
            if(itt->position + 2 < _rules[itt->rule_id].size())
                syms = first(_rules[itt->rule_id][itt->position + 2]);
            else{
                Symbol sym = {true, itt->nextchar_id};
                syms.insert(sym);
            }

            for(set<Symbol>::iterator ittt = syms.begin(); ittt != syms.end(); ittt++){
                //For each b in first(ba)

                LRItem item = {int(i), 0, ittt->index};
                pair<LRItemsType::iterator, bool> ret = enclosure.insert(item);
                if(ret.second)
                    s.push(ret.first);
            }

        }
    }

    if(!enclosure.empty()){
        pair<LRItemsSetType::iterator, bool> ret = _lrItemsSet.insert(enclosure);
        if(ret.second){
            _lrItemsId.push_back(&(*ret.first));
        }
        return ret.first;
    }
    return _lrItemsSet.end();
}

bool ParserBuilder::parsefile(const char* filename){
    _terminals.clear();
    _nonTerminals.clear();
    _rules.clear();

    ifstream dparse_file(filename, ios::in);
    if(!dparse_file){
        _errno = 1;
        return false;
    }

    string dlex_filename, lua_filename;
    getline(dparse_file, dlex_filename);
    getline(dparse_file, lua_filename);

    luaL_dofile(_parser.L, lua_filename.c_str());

    ifstream dlex_file(dlex_filename.c_str(), ios::in);

    if(!dlex_file){
        _errno = 2;
        return false;
    }

    string line;
    while(getline(dlex_file, line)){
        stringstream ss;
        ss << line;

        int no;
        ss >> no >> line;
        _terminals.insert(make_pair(line, no));
    }

    vector<vector<string> > temp_rules;

    string c;
    int index = 0;
    _nonTerminals.push_back("dparse_S");
    _nonTerminalsMap.insert(make_pair("dparse_S", index++));
    Symbol x = {false, 0};
    Symbol y = {false, 1};

    vector<Symbol> vec;
    vec.push_back(x);
    vec.push_back(y);
    _rules.push_back(vec);
    while(dparse_file>>c){
        string name, sub;
        vector<string> temp_rule;
        switch (c[0]){
        case ':':
            dparse_file >> name;
            _nonTerminals.push_back(name);
            _nonTerminalsMap.insert(make_pair(name, index++));
            getline(dparse_file, sub);
            while(getline(dparse_file, sub)){
                if(sub[0] == ';')
                    break;
                if(sub[0] == '#')
                    continue;

                temp_rule.clear();
                temp_rule.push_back(name);

                stringstream ss;
                ss<<sub;
                while(ss >> sub){
                    if(sub == ":"){
                        ss >> sub;
                        vector<string>::iterator ret = find(_parser.actionFunctions.begin(),
                                                            _parser.actionFunctions.end(),
                                                            sub);
                        if(ret == _parser.actionFunctions.end()){
                            _parser.actionFunctions.push_back(sub);
                            _rulesFunction.push_back(_parser.actionFunctions.size() - 1);
                        }else{
                            _rulesFunction.push_back(ret - _parser.actionFunctions.begin());
                        }
                        break;
                    }
                    temp_rule.push_back(sub);
                }
                temp_rules.push_back(temp_rule);
            }
            break;
        case '#':
            getline(dparse_file, sub);
            break;
        }
    }

    for(unsigned int i = 0; i < temp_rules.size(); i++){
        vector<Symbol> rule;
        for(unsigned int j = 0; j < temp_rules[i].size(); j++){
            map<string, int>::iterator it;
            if((it = _terminals.find(temp_rules[i][j])) != _terminals.end()){
                Symbol sym = {true, it->second};
                rule.push_back(sym);
            }else if((it = _nonTerminalsMap.find(temp_rules[i][j])) != _nonTerminalsMap.end()){
                Symbol sym = {false, it->second};
                rule.push_back(sym);
            }else{
                _errno = 3;
                cout<<temp_rules[i][j]<<endl;
                return false;
            }
        }
        _rules.push_back(rule);
    }
    _errno = 0;
    return true;
}
