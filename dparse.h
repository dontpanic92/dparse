#ifndef DPARSE_H
#define DPARSE_H
#include <map>
#include <vector>
#include <string>
#include <set>
#include "FiniteAutomata.h"
using namespace std;

struct ActionKey{
    int state;
    int next_terminal_id;
    bool operator < (const ActionKey& c) const {
        if(state != c.state)
            return state < c.state;
        return next_terminal_id < c.next_terminal_id;
    }
};

enum ActionType{Shift, Reduce, Acc};

struct ActionValue{
    int action;
    int extra;      //Shift: which state to push into stack; Reduce: Rules Length
    int extra2;     //Shift: Not use; Reduce: Non-terminal ID
    int extra3;     //Shift: Not use; Reduce: Function Name ID
};

struct GotoKey{
    int state;
    int next_nonterminal_id;

    bool operator < (const GotoKey& c) const {
        if(state != c.state)
            return state < c.state;
        return next_nonterminal_id < c.next_nonterminal_id;
    }
};

/*struct GotoValue{
    int next_state;
    int function_id;
};*/

struct lua_State;
struct Parser{
    map<ActionKey, ActionValue> actionTable;
    map<GotoKey, int> gotoTable;
    vector<string> actionFunctions;
    lua_State* L;
};

class ParserRunner{
public:
    ParserRunner(LexerRunner* lexer);

    void setParser(Parser* parser);

    bool parse();

private:
    LexerRunner* _lexer;
    Parser* _parser;

    struct State{
        int state;
        int val;

        State(int s, int v){state = s, val = v;}
    };

    vector<State> _stk;
    Token _prevToken;

    static int getstackcontent(lua_State* L);
    static int getcurrenttokenname(lua_State* L);
};


class ParserBuilder{
public:
    ParserBuilder(lua_State* L){_parser.L = L;}
    Parser* buildFromFile(const char* filename);

private:
    bool parsefile(const char* filename);

    struct LRItem{
        int rule_id;
        int position;
        int nextchar_id;

        bool operator <(const LRItem& i) const{
            if(rule_id != i.rule_id)
                return (rule_id < i.rule_id);
            if (position != i.position)
                return (position < i.position);
            return (nextchar_id < i.nextchar_id);
        }
    };

    typedef set<set<LRItem> > LRItemsSetType;
    LRItemsSetType _lrItemsSet;

    typedef set<LRItem> LRItemsType;

    vector<const LRItemsType*> _lrItemsId;

    struct Symbol{
        bool is_terminal;
        int index;

        bool operator ==(const Symbol& s) const {return (s.is_terminal == is_terminal) && (s.index == index);}
        bool operator !=(const Symbol& s) const {return ! operator ==(s);}
        bool operator <(const Symbol& s) const {
            if(is_terminal == s.is_terminal)
                return (index < s.index);
            else
                return is_terminal < s.is_terminal;
        }
    };

    struct PrivateGotoKey{
        //const LRItemsType* pointer;
        int id;
        Symbol next_symbol;

        bool operator < (const PrivateGotoKey& c) const {
            if(id != c.id)
                return id < c.id;
            return next_symbol < c.next_symbol;
        }
    };

    map<PrivateGotoKey, int> _gotoTable;

    void calcItemsSetAndGotoTable();

    void calcActionAndGotoTable();

    set<Symbol> first(const Symbol &sym, vector<int> &visited);

    set<Symbol> first(const Symbol& sym);

    int findItemsSetId(LRItemsSetType::iterator it);

    LRItemsSetType::iterator gotonext(LRItemsSetType::iterator it, const Symbol& sym);

    LRItemsSetType::iterator enclosure(const LRItemsType &itemset);

    map<string, int> _terminals;

    vector<string>  _nonTerminals;
    map<string, int> _nonTerminalsMap;

    vector<vector<Symbol> > _rules;

    vector<int> _rulesFunction;

    Parser _parser;

    int _errno;
};

#endif // DPARSE_H
