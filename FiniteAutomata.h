#ifndef __YASTC_FINITEAUTOMATA__
#define __YASTC_FINITEAUTOMATA__
#include <vector>
#include <iostream>
#include <string>
#include <stack>
#include <set>
#include <list>
#include <map>
using namespace std;

class DFAFeeder{
public:
    DFAFeeder(const char* buffer, unsigned long length):_buffer(buffer), _length(length){ _curPos = 0;}
    char getNextChar(){return _buffer[_curPos++];}
    char peekNextChar(){return _buffer[_curPos];}
    bool hasNextChar(){if(_curPos >= _length) return false; return true;}
    void putBack(){if(_curPos > 0) _curPos--;}
    char getChar(unsigned long pos){return _buffer[pos];}
    unsigned long getPos(){return _curPos;}
private:
    const char* _buffer;
    unsigned long _length;
    unsigned long _curPos;
};

typedef int (*PFNDFAACTION)(int state, int character, int* nextState, void* pThis);


struct DFATransfer{
    int input;
    int nextState;
};

enum Action{Accept, Omit};

struct DFAState{
    bool isTerminal;
    int  term_id;
    int  action;
};

class DFARunner{
public:
    DFARunner(DFAFeeder* feeder, void* userdata){_feeder = feeder; _curState = _prevState = 0;_userdata = userdata;}
    ~DFARunner(){}
    void setTransTable(const vector<list<DFATransfer> >& trans){_DFATransTable = trans;}
    void setDFAStates(const vector<DFAState>& dfastates){_DFAStates = dfastates;}
    void init(int startState){_prevState = _curState = startState;}
    void goBack(){_feeder->putBack();}

    int step();
    int getCurrentState(){return _curState;}
    int getPrevState(){return _prevState;}

protected:
    DFARunner(const DFARunner&){}

    DFAFeeder* _feeder;
    void* _userdata;

    vector<list<DFATransfer> > _DFATransTable;
    vector<DFAState> _DFAStates;
    //vector<DFATransfer>        _DFADefaultTransTable;

    int _curState;
    int _prevState;
};

struct Lexer{
    vector<DFAState> states;
    vector<list<DFATransfer> > trans;
};

const int FakeState = 0;

namespace LexerState {
enum LexerAction{
    EOF = -1,
    Error = -2,
    //FallBackAndAccept = -3,
    NeedToContinue = -4
};
}

struct Range{
    unsigned long start;
    unsigned long end;
    int tokenType;
};

struct Token{
    const char* token;
    int tokenType;
};

class LexerRunner : protected DFARunner{
public:
    LexerRunner(DFAFeeder* feeder):DFARunner(feeder, this){_curTokenPos = 0;DFARunner::init(FakeState);}
    void setLexer(Lexer* lexer);
    Token getNextToken();
    Token peekNextToken();
    Token peekPrevToken();
    void  putBack();
protected:
    void copyToken(const Range& range);
    Token rangeToToken(const Range& range);

    char _tokenBuffer[256];
    Range	_curToken;
    unsigned int _curTokenPos;
    vector<Range> _tokens;
    Lexer* _lexer;
};

class LexerBuilder;

class RegularExpression{
public:
    RegularExpression(const string & str, int term_id, int action){
        re_str = str;
        root = NULL;
        _term_id = term_id;
        _action = action;
    }

    ~RegularExpression(){if(root) recursiveDeleteTree(&root);}

    string str(){return re_str;}

    bool compile(){
        if(root)
            recursiveDeleteTree(&root);

        if(!preProcess() || !buildTree()){
            //cout<<"Grammar Error"<<endl;
            return false;
        }
        calcPos(root);

        for(set<BinaryTreeNode*>::iterator it = root->firstpos.begin(); it != root->firstpos.end(); it++){
            (*it)->is_start_state = true;
        }

        for(set<BinaryTreeNode*>::iterator it = root->lastpos.begin(); it != root->lastpos.end(); it++){
            (*it)->is_term_state = true;
            (*it)->action = _action;
            (*it)->term_id = _term_id;
        }

        return true;
    }

protected:
    struct BinaryTreeNode{
        BinaryTreeNode* leftChild;
        BinaryTreeNode* rightChild;
        char			character;

        bool            is_start_state;
        bool            is_term_state;
        int             term_id;
        int             action;

        bool                    nullable;
        set<BinaryTreeNode*>    firstpos;
        set<BinaryTreeNode*>    lastpos;
        set<BinaryTreeNode*>    followpos;

        BinaryTreeNode(){leftChild = rightChild = NULL;character = '\0';nullable = false;is_start_state = is_term_state = false;}
        BinaryTreeNode(BinaryTreeNode* left, BinaryTreeNode* right, char character){
            leftChild = left; rightChild = right; this->character = character;nullable = false;
            is_start_state = is_term_state = false;
        }
    };

    bool preProcess();
    bool preProcessCharset(string::iterator& it);
    char processEscape(int& start);

    BinaryTreeNode* buildTree();
    void recursiveDeleteTree(BinaryTreeNode **r);

    bool processOp(stack<BinaryTreeNode*>& op, stack<BinaryTreeNode*>& ch);
    void addOp(char c, stack<BinaryTreeNode*>& op, stack<BinaryTreeNode*>& ch);

    void calcPos(BinaryTreeNode* cur);

    void postOrder(const BinaryTreeNode *TreeNode);

    bool is_op(char ch){return (ch == '|') || (ch == '.') || (ch == '*')|| (ch == '(')|| (ch == ')');}
    int get_op_pri(char ch){if(ch == '(') return -2;if(ch == ')') return -1; if(ch == '|') return 0; if(ch == '.') return 1; if(ch == '*')return 2; return 3;}
    int get_op_operandnum(char op){if(op == '|' || op == '.') return 2;return 1;}


    vector<BinaryTreeNode*> leafnodes;
    BinaryTreeNode* root;
    string re_str;
    string postfix_str;

    int _term_id, _action;
    friend class LexerBuilder;
};

class LexerBuilder{
public:
    LexerBuilder(vector<RegularExpression*> & regex):_regex(regex){}
    ~LexerBuilder(){freeNodes();}

    Lexer* build();
    int getErrorLine(){return _error_line;}

private:
    void makeDFA();

    typedef RegularExpression::BinaryTreeNode BinaryTreeNode;

    BinaryTreeNode* allocateNode(BinaryTreeNode* left, BinaryTreeNode* right, char character){
        BinaryTreeNode* node = new BinaryTreeNode(left, right, character);
        _newlyAllocatedNodes.push_back(node);
        return node;
    }
    void freeNodes(){
        for(int i = 0; i < _newlyAllocatedNodes.size(); i++){
            delete _newlyAllocatedNodes[i];
        }
        _newlyAllocatedNodes.clear();
    }

    set<set<BinaryTreeNode* > > dstates_unsigned;
    set<set<BinaryTreeNode* > > dstates_signed;

    struct DTrans{
        set<BinaryTreeNode*>  from_state;
        char                  character;
        set<BinaryTreeNode*>  to_state;

        DTrans(const set<BinaryTreeNode*>& _from_state, char _character, const set<BinaryTreeNode*>& _to_state)
            :from_state(_from_state), character(_character), to_state(_to_state){}
    };
    vector<DTrans> dtrans;

    //vector<set<BinaryTreeNode> *> state_map;
    vector<BinaryTreeNode*> _newlyAllocatedNodes;

    BinaryTreeNode* root;
    vector<RegularExpression*> & _regex;
    Lexer _lexer;
    int _error_line;
};

#endif
