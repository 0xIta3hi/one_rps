// this file will be used currently write a simple tcp server in cpp. afterwards i will implement following things
/*
1. ability to write, read, update, delete from a file which will be the temporary database.
2. Convert the tcp server to a proper api with endpoints for CRUD operations.
3. scale the API to handle at first handle just 10k requests.
4. scale the api further to handle 1M requests per second.
*/

// normal practicing c++
#include<iostream>
using namespace std;

void change(int x, int& y){
    x = 100;
    y = 200;
}

// function overloading
int add(int a, int b){
    std::cout << "int add called\n";
    return a + b;
}

double add(double a, double b){
    std::cout << "double add called\n";
    return a + b;
}

string add(string a, string b){
    std::cout << "string add called\n";
    return a + b;
}

int main(){
    int a = 10;
    int b = 20;

    change(a,b);
    std::cout << a << " " << b << "\n";
    add(10, 20);
    add(10.5, 20.5);
    add("Hello ", "World");
}
