#pragma once
/*
int8_t      : typedef signed char;
uint8_t    : typedef unsigned char;
*/
//Timestamp 时间戳
#include<iostream>
#include<string>
class Timestamp
{
private:
   int64_t microSecondsSinceEpoch_;
public:
    Timestamp();
    explicit Timestamp(int64_t microSecondsSinceEpoch); //explicit声明的构造函数不能在隐式转换中使用
    /*
     classA = B;   如果B满足classA的某个构造函数，就会调用classA的构造函数，创建一个classA的对象
     explicit就是为了避免Timestamp A = 12; 12发生隐式转换的情况
    */
    //~Timestamp();
    static Timestamp now();
    std::string toString() const;
}; 

