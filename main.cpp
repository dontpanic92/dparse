
#include "dparse.h"
#include "FiniteAutomata.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>


extern "C"{
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

using namespace std;

lua_State* L;

char str[10000];

int main()
{

    ifstream in("/home/dontpanic/1.dlex", ios::in);
    ifstream in2("/home/dontpanic/testfile", ios::in);
    in2.seekg(0, ios::end);
    unsigned long len = in2.tellg();
    in2.seekg(0, ios::beg);
    in2.read(str, 10000);

    vector<RegularExpression*> v;

    int id;

    string action, x;
    while(in>>id>>x>>action){
        int iAction;
        string str;
        if(action == "Accept")
            iAction = Accept;
        else
            iAction = Omit;
        getline(in, str);
        str.erase(str.begin());
        //cout<<str<<endl;
        RegularExpression* regex = new RegularExpression(str, id, iAction);
        v.push_back(regex);
    }
    //RegularExpression* regex = new RegularExpression("\"[ _a-zA-Z0-9!%\\\\]*\"", 0, Accept);
    //v.push_back(regex);

    LexerBuilder builder(v);
    Lexer* lexer = builder.build();

    if(!lexer){
        cout<<"Grammar Error"<<endl;
        cout<<"Error Line:"<<builder.getErrorLine()<<endl;
        return 0;
    }

    DFAFeeder feeder(str, len);

    LexerRunner runner(&feeder);
    runner.setLexer(lexer);



    L = luaL_newstate();
    luaL_openlibs(L);

    ParserBuilder pfile(L);
    Parser* parser = pfile.buildFromFile("/home/dontpanic/1.dparse");

    ParserRunner parserRunner(&runner);

    parserRunner.setParser(parser);

    int i = 0;
    while(parserRunner.parse())
        i++;

    cout<<"parser end at " << i <<endl;

    for(int i = 0; i < v.size(); i++){
        delete v[i];
    }


    lua_close(L);

    return 0;

}

