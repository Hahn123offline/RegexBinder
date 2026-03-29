// #include "SimpleBinder-v0.0.hpp"
#include "ver0.1/RegexBinder-v0.1.hpp"
#include<string>
#include<functional>

/*
// 1. 捕获一个单词
R"((\w+))"           // hello, world123, test_abc

// 2. 捕获一个数字
R"((\d+))"           // 123, 456, 0

// 3. 捕获一个带空格的短语（到行尾）
R"((.*)$)"           // "hello world", "test 123"

// 4. 捕获两个单词
R"((\w+)\s+(\w+))"   // "hello world" → matches[1]="hello", matches[2]="world"

// 5. 捕获引号内的内容
R"("([^"]*)")"       // "hello" → matches[1]="hello"

// 6. 捕获邮箱地址（简单版）
R"((\w+@\w+\.\w+))"  // "test@example.com"

// 7. 捕获路径（允许空格和斜杠）
R"(([\/\w\s\.]+))"   // "folder/sub folder/file.txt"
*/

/// <summary>
/// 任务清单：
/// 0.全局单例 （搞定！）
/// 1.添加帮助系统，help查询所有命令，help command查询特定功能！（搞定！）
/// 1.1支持错误处理函数
/// **2.自动类型推导，不用function包装函数（包括lambda）！需要模板类型推导+decltype（搞定！）
/// **3.支持成员函数，传入对象地址+类内函数地址，帅！（搞定！）
/// **3.1支持可变参数模板，无需重载（搞定！）
/// 3.2支持队列系统
/// 3.3支持队列条件执行系统
/// *4.支持命令链
/// *5.支持命令别名
/// (6.变量支持，set_var "a" "b"储存一个小值。这是干嘛用的)（彩蛋功能）
/// 7.单元测试assert
/// 8.性能优化，unordered map辅助查找（存疑）+ *缓存编译后的正则
/// 9.可能该是写一个文档了
/// 10.万一能放到github上面呢
/// </summary>
/// <param name="s"></param>

class testClass
{
public:
	void function_in_class()
	{
		std::cout << "yee!" << std::endl;
	}
};

void echo(std::string s)
{
	std::cout << s << std::endl;
}

int main()
{
	auto& binder = RegexBinder::global();

	binder.bind("add (\\d+) (\\d+)",
		[](int a, int b) {
			std::cout << a << " + " << b << " = " << a + b << "\n";
		});
	
	binder.bind("echo (.+)", echo, "the first function!");

	testClass c;
	binder.bind("test", &c, &testClass::function_in_class, "你知道吗这很厉害，这是一个说明。这个函数是一个成员函数，调用会yee一下"); // 支持成员函数

	binder.list();
	binder.run_interactive();
}