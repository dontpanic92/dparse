#include "FiniteAutomata.h"
#include <string.h>
#include <stack>
#include <algorithm>
using namespace std;

int DFARunner::step(){
    int input;
    if(!_feeder->hasNextChar())
        return LexerState::EOF;
    input = _feeder->getNextChar();

    for(list<DFATransfer>::iterator it = _DFATransTable[_curState].begin(); it != _DFATransTable[_curState].end(); it++){
        if(it->input == input){
            _prevState = _curState;
            _curState = it->nextState;
            return LexerState::NeedToContinue;
        }
    }
    return LexerState::Error;
}


Token LexerRunner::rangeToToken(const Range& range){
    Token t;
    copyToken(range);
    t.token = _tokenBuffer;
    t.tokenType = range.tokenType;
    return t;
}

void LexerRunner::copyToken(const Range& range){
    int i = 0;
    for(; i <= range.end - range.start; i++){
        _tokenBuffer[i] = _feeder->getChar(i + range.start);
    }
    _tokenBuffer[i] = '\0';
}

Token LexerRunner::peekNextToken(){
    Token temp = getNextToken();
    //if(temp.token != NULL){
    putBack();
    //}
    return temp;
}

Token LexerRunner::peekPrevToken(){
    if(_curTokenPos > 0){
        return rangeToToken(_tokens[_curTokenPos - 1]);
    }
    Token token;
    token.token = NULL;
    return token;
}

void  LexerRunner::putBack(){
    if(_curTokenPos > 0)
        _curTokenPos--;
}

Token LexerRunner::getNextToken(){
    if(_curTokenPos < _tokens.size()){
        return rangeToToken(_tokens[_curTokenPos++]);
    }
    int state;
    Range newRange;
    newRange.start = (unsigned long)(-1);
    Token t;
    t.token = NULL;
    _curTokenPos++;
    while((state = step())){
        switch(state){
        case LexerState::NeedToContinue:
            if(state == LexerState::NeedToContinue){
                int state2 = DFARunner::getCurrentState();
                if(state2 != FakeState && newRange.start == (unsigned long )-1)
                    newRange.start = _feeder->getPos() - 1;
            }
            break;
        case LexerState::Error:
        case LexerState::EOF:
            if(_DFAStates[DFARunner::getCurrentState()].isTerminal){
                //Accept
                if(state == LexerState::Error)
                    DFARunner::goBack();
                if(_DFAStates[DFARunner::getCurrentState()].action == Accept){
                    newRange.end = _feeder->getPos() - 1;
                    newRange.tokenType = _DFAStates[DFARunner::getCurrentState()].term_id;
                    _tokens.push_back(newRange);
                    DFARunner::init(FakeState);
                    return rangeToToken(newRange);
                }else{
                    DFARunner::init(FakeState);
                    newRange.start = (unsigned long)(-1);
                }
            }else{
                if(DFARunner::getCurrentState() != 0)
                    DFARunner::goBack();
                DFARunner::init(FakeState);
                newRange.start = (unsigned long)(-1);
            }
            if(state == LexerState::EOF)
                return t;
            break;
        default:
            break;
        }
    }
    return t;
}

void LexerRunner::setLexer(Lexer* lexer){
    _lexer = lexer;
    DFARunner::setDFAStates(lexer->states);
    DFARunner::setTransTable(lexer->trans);
}

bool RegularExpression::processOp(stack<BinaryTreeNode*>& op, stack<BinaryTreeNode*>& ch){
    if(op.top()->character == '*'&& ch.size() > 0){
        BinaryTreeNode* node = op.top();
        op.pop();
        node->leftChild = ch.top();
        ch.pop();
        ch.push(node);
        return true;
    }else if((op.top()->character == '|' || op.top()->character == '.') && ch.size() > 1){
        BinaryTreeNode* node = op.top();
        op.pop();
        node->rightChild = ch.top();
        ch.pop();
        node->leftChild = ch.top();
        ch.pop();
        ch.push(node);
        return true;
    }else if(op.top()->character == '('){
        op.pop();
        return true;
    }
    return false;
}

bool RegularExpression::preProcessCharset(string::iterator& it){
    list<char> charset;
    string::iterator itt = it + 1;

    int state = 0;

    for(; *itt != ']'; itt++){
        switch(state){
        case 0:
            if(*itt == '-'){
                if(charset.empty())
                    return false;
                state = 1;
            }else{
                charset.push_back(*itt);
            }
            break;
        case 1:
            if(*itt < charset.back())
                return false;
            for(char i = charset.back() + 1; i <= *itt; i++){
                charset.push_back(i);
            }
            state = 0;
            break;
        }
    }

    if(state != 0)
        return false;

    string str;
    str.reserve(charset.size() * 2 + 2);
    str += "(";
    char last = '(';
    for(list<char>::iterator itt2 = charset.begin(); itt2 != charset.end(); itt2++){
        if(itt2 != charset.begin() && last != '\\')
            str += '|';
        str += *itt2;
        last = *itt2;
    }
    str += ")";
    re_str.erase(it, itt + 1);
    re_str.insert(it, str.begin(), str.end());
    it = re_str.begin();
    return true;
}

bool RegularExpression::preProcess(){
    for(string::iterator it = re_str.begin(); it != re_str.end();){
        switch(*it){
        case '\\':
            it++;
            if(it == re_str.end())
                throw "";
            it++;
            break;
        case '[':
            if(!preProcessCharset(it))
                return false;
            break;
        default:
            it++;
            break;
        }
    }

    return true;
}


char RegularExpression::processEscape(int& start){
    start++;
    if(start >= re_str.size())
        throw "";

    switch(re_str[start]){
    case 'n':
        return '\n';
        break;
    case 't':
        return '\t';
        break;
    default:
        return re_str[start];
        break;
    }
    return true;
}

RegularExpression::BinaryTreeNode* RegularExpression::buildTree(){
    stack<BinaryTreeNode*> op;
    stack<BinaryTreeNode*> ch;
    int start = 0;
    bool last_is_char = false;
    try{
        while(start < re_str.length()){
            char c = re_str[start];
            if(is_op(c)){
                if(c == '(' && last_is_char)
                    addOp('.', op, ch);
                addOp(c, op, ch);
                if(c == '(' || c == '|')
                    last_is_char = false;
                start++;
            }else{
                BinaryTreeNode* node;
                if(c == '\\'){
                    //start++;
                    node = new BinaryTreeNode(0, 0, processEscape(start));
                }else{
                    node = new BinaryTreeNode(0, 0, c);
                }

                if(last_is_char == true){
                    addOp('.', op, ch);
                }
                ch.push(node);
                leafnodes.push_back(node);
                last_is_char = true;
                start++;
            }

        }
        while(op.size() > 0 && op.top()->character != '(' && processOp(op, ch));

        if(op.size() != 0 || ch.size() != 1){
            throw "";
        }

        op.push(new BinaryTreeNode(0, 0, '.'));
        BinaryTreeNode* n = new BinaryTreeNode(0, 0, '#');
        n->is_term_state = true;

        ch.push(n);
        processOp(op, ch);
    }catch(...){
        while(op.size() > 0){
            BinaryTreeNode* node = op.top();
            recursiveDeleteTree(&node);
            op.pop();
        }
        while(ch.size() > 0){
            BinaryTreeNode* node = ch.top();
            recursiveDeleteTree(&node);
            ch.pop();
        }
        return NULL;
    }

    root = ch.top();
    return root;
}

void RegularExpression::addOp(char c, stack<BinaryTreeNode*>& op, stack<BinaryTreeNode*>& ch){
    while(c != '(' && op.size() > 0 && get_op_pri(c) <= get_op_pri(op.top()->character) && processOp(op, ch));

    if(c != ')')
        op.push(new BinaryTreeNode(0, 0, c));
    else if(op.empty() || op.top()->character != '(')
        throw "";
    else
        processOp(op, ch);
}

void RegularExpression::recursiveDeleteTree(BinaryTreeNode** r){
    if (*r == NULL)
        return;
    recursiveDeleteTree(&(*r)->leftChild);
    recursiveDeleteTree(&(*r)->rightChild);
    delete (*r);
    *r = NULL;
}

void RegularExpression::postOrder(const BinaryTreeNode *TreeNode){
    if (TreeNode == NULL)
        return;
    postOrder(TreeNode->leftChild);
    postOrder(TreeNode->rightChild);
    postfix_str += TreeNode->character;
    postfix_str += " ";
}

void RegularExpression::calcPos(BinaryTreeNode* cur){
    if(cur->leftChild)
        calcPos(cur->leftChild);
    if(cur->rightChild)
        calcPos(cur->rightChild);


    cur->firstpos.clear();
    cur->lastpos.clear();


    if(!cur->leftChild && !cur->rightChild){ //leaf node
        cur->firstpos.insert(cur);
        cur->lastpos.insert(cur);
    }else if(cur->character == '.'){
        cur->nullable = (cur->leftChild->nullable && cur->rightChild->nullable);

        if(cur->leftChild->nullable)
            set_union(cur->leftChild->firstpos.begin(), cur->leftChild->firstpos.end(),
                      cur->rightChild->firstpos.begin(), cur->rightChild->firstpos.end(),
                      inserter(cur->firstpos, cur->firstpos.begin()));
        else
            cur->firstpos = cur->leftChild->firstpos;

        if(cur->rightChild->nullable)
            set_union(cur->leftChild->lastpos.begin(), cur->leftChild->lastpos.end(),
                      cur->rightChild->lastpos.begin(), cur->rightChild->lastpos.end(),
                      inserter(cur->lastpos, cur->lastpos.begin()));
        else
            cur->lastpos = cur->rightChild->lastpos;


        for(set<BinaryTreeNode*>::iterator it = cur->leftChild->lastpos.begin(); it != cur->leftChild->lastpos.end(); it++){
            set_union((*it)->followpos.begin(), (*it)->followpos.end(),
                      cur->rightChild->firstpos.begin(), cur->rightChild->firstpos.end(),
                      inserter((*it)->followpos, (*it)->followpos.begin()));
        }

    }else if(cur->character == '|'){
        cur->nullable = (cur->leftChild->nullable || cur->rightChild->nullable);
        set_union(cur->leftChild->firstpos.begin(), cur->leftChild->firstpos.end(),
                  cur->rightChild->firstpos.begin(), cur->rightChild->firstpos.end(),
                  inserter(cur->firstpos, cur->firstpos.begin()));
        set_union(cur->leftChild->lastpos.begin(), cur->leftChild->lastpos.end(),
                  cur->rightChild->lastpos.begin(), cur->rightChild->lastpos.end(),
                  inserter(cur->lastpos, cur->lastpos.begin()));

    }else if(cur->character == '*'){
        cur->nullable = true;
        cur->firstpos = cur->leftChild->firstpos;
        cur->lastpos = cur->leftChild->lastpos;

        for(set<BinaryTreeNode*>::iterator it = cur->leftChild->lastpos.begin(); it != cur->leftChild->lastpos.end(); it++){
            set_union((*it)->followpos.begin(), (*it)->followpos.end(),
                      cur->firstpos.begin(), cur->firstpos.end(),
                      inserter((*it)->followpos, (*it)->followpos.begin()));
        }
    }else
        throw;
}

void LexerBuilder::makeDFA(){
    dstates_unsigned.insert(root->firstpos);


    while(!dstates_unsigned.empty()){
        set<set<BinaryTreeNode* > >::iterator it = dstates_unsigned.begin();
        set<char> chars;
        for(set<BinaryTreeNode* >::iterator itt =  it->begin(); itt != it->end(); itt++){ //for each input character
            if(chars.find((*itt)->character) != chars.end())
                continue;

            set<BinaryTreeNode* > newstate;
            for(set<BinaryTreeNode* >::iterator itt1 =  it->begin(); itt1 != it->end(); itt1++){ //for each element in set
                if((*itt1)->character != (*itt)->character)
                    continue;
                for(set<BinaryTreeNode* >::iterator itt2 =  (*itt1)->followpos.begin(); itt2 != (*itt1)->followpos.end(); itt2++){
                    //for each element in followpos of each element in set
                    //insert it into our newstate
                    newstate.insert(*itt2);
                }
            }

            if(!newstate.empty()){
                chars.insert((*itt)->character);
                if(dstates_signed.find(newstate) == dstates_signed.end()){
                    dstates_unsigned.insert(newstate);
                }

                dtrans.push_back(DTrans(*it, (*itt)->character, newstate));
            }
        }
        dstates_signed.insert(*it);
        dstates_unsigned.erase(it);
    }
}

Lexer* LexerBuilder::build(){
    _lexer.states.clear();
    _lexer.trans.clear();
    freeNodes();
    if(_regex.size() == 0){
        return NULL;
    }

    if(!_regex[0]->compile()){
        _error_line = 0;
        return NULL;
    }
    root = _regex[0]->root;
    for(int i = 1; i < _regex.size(); i++){
        if(!_regex[i]->compile()){
            _error_line = i;
            return NULL;
        }
        BinaryTreeNode* newnode = allocateNode(root, _regex[i]->root, '|');

        newnode->nullable = (newnode->leftChild->nullable || newnode->rightChild->nullable);
        set_union(newnode->leftChild->firstpos.begin(), newnode->leftChild->firstpos.end(),
                  newnode->rightChild->firstpos.begin(), newnode->rightChild->firstpos.end(),
                  inserter(newnode->firstpos, newnode->firstpos.begin()));
        set_union(newnode->leftChild->lastpos.begin(), newnode->leftChild->lastpos.end(),
                  newnode->rightChild->lastpos.begin(), newnode->rightChild->lastpos.end(),
                  inserter(newnode->lastpos, newnode->lastpos.begin()));

        root = newnode;
    }

    makeDFA();

    map<set<BinaryTreeNode*>, int> statesMap;
    int i = 0;
    for(set<set<BinaryTreeNode* > >::iterator it = dstates_signed.begin(); it != dstates_signed.end(); it++){
        for(set<BinaryTreeNode* >::iterator itt = it->begin(); itt != it->end(); itt++){
            DFAState state = {false, 0, Accept};
            if((*itt)->is_start_state){
                statesMap.insert(make_pair(*it, i++));
                _lexer.states.push_back(state);
                list<DFATransfer> newlist;
                _lexer.trans.push_back(newlist);
                dstates_signed.erase(it);
                goto next;
            }
        }
    }

next:
    for(set<set<BinaryTreeNode* > >::iterator it = dstates_signed.begin(); it != dstates_signed.end(); it++){
        statesMap.insert(make_pair(*it, i++));
        DFAState state = {false, 0, Accept};
        for(set<BinaryTreeNode* >::iterator itt = it->begin(); itt != it->end(); itt++){
            if((*itt)->is_term_state){
                state.isTerminal = true;
                state.term_id = (*itt)->term_id;
                state.action = (*itt)->action;
                break;
            }
        }
        _lexer.states.push_back(state);
        list<DFATransfer> newlist;
        _lexer.trans.push_back(newlist);
    }

    for(vector<DTrans>::iterator it = dtrans.begin(); it != dtrans.end(); it++){
        DFATransfer tran = {it->character, statesMap.find(it->to_state)->second};
        _lexer.trans[statesMap.find(it->from_state)->second].push_back(tran);
    }

    return &_lexer;
}
