#pragma once

// v0.0
//这是第一个开始重构的版本。目标：掌握当前的模板用法，能理解整个类的基础结构，以及正则表达式工作原理

// 抄写开始：

#include<iostream>
#include<functional> // 核心部分
#include<map>
#include<string>
#include<vector>
#include<regex> // 正则表达式识别
#include<sstream>

// 参数解析
template<typename T> // 模板编程
T parse_arg(const std::string& s) // 返回T类型数据，接受一个字符串
{
	if constexpr (std::is_same_v<T, int>) return std::stoi(s);
	else if constexpr (std::is_same_v<T, double>) return std::stod(s);
	else if constexpr (std::is_same_v<T, std::string>) return s;
	else if constexpr (std::is_same_v<T, bool>) return s == "true" || s == "1";
	else
	{
		static_assert(sizeof(T) == 0, "不支持的类型");
        // 断言检查：如果条件为假，编译器停止编译，报错自定义的信息↑
        // assert普通断言在运行时检查
	}
}
// 为什么使用常量表达式？原因：这个库是在编译前就完成它的工作的。
// 使用constexpr能在编译时就决定绑定的代码，实现某种优化（？
// 类型特质is_same_v是一个is_same<>::value简化写法（cpp17），传入两个参数类型判断是否一样





class Command // 指令构成
{
private:
	std::regex pattern;
	std::function<void(const std::vector<std::string>&)> handler;
    std::string description;
    std::string pattern_str;

public:
	Command(const std::string& pat, std::function<void(const std::vector<std::string>&)> h
    , const std::string& desc = "")
		: pattern(pat), handler(h), description(desc), pattern_str(pat) { }
       // 注意：这里什么都没执行！↑，给handler传入h只是让它知道规则
       // 只是把正则模式和如何执行的指令保存起来

    std::string get_description() { return description; }
    std::string get_pattern() { return pattern_str; }

    // 纯匹配：只判断是否匹配，不执行
    bool matches(const std::string& input, std::vector<std::string>& args) const
    {
        std::smatch match_results;
        if (std::regex_match(input, match_results, pattern))
        {
            args.clear();
            for (size_t i = 1; i < match_results.size(); i++)
            {
                args.push_back(match_results[i].str());
            }
            return true;
        }
        return false;
    }

    // 执行：用参数执行 handler
    void execute(const std::vector<std::string>& args) const
    {
        handler(args);
    }

    // 快速匹配（不带参数，用于 help 查找）
    bool matches(const std::string& input) const
    {
        return std::regex_match(input, pattern);
    }
};

class RegexBinder
{
private:
	std::vector<Command> commands;

    

public:

    /*  万能版本，你再也不需要下面那些了  */
    template<typename Func>
    void bind(const std::string& pattern,
        Func&& func,                       // 完美转发
        const std::string& desc = "")
    {

        using traits = function_traits<std::decay_t<Func>>;
        constexpr size_t arity = traits::arity;  // 参数个数（编译时确定）

        // 根据参数个数，生成对应的索引序列 0,1,2,...,arity-1
        // 然后调用 bind_impl
        bind_impl(pattern, std::forward<Func>(func), desc,
            std::make_index_sequence<arity>{});
    }
    /*  万能版本  */

    // 运行一次命令
    bool run(const std::string& input)
    {
        for (auto& cmd : commands)
        {
            std::vector<std::string> args;
            if (cmd.matches(input, args)) // 这个函数，就是前面理解的那个，通过用户输入的input匹配要执行的命令
            {
                cmd.execute(args);
                return true; // 退出，因为当前匹配到的函数已经被执行，没必要继续遍历查找。
            }
        }
        std::cout << "未知命令: " << input << "\n";
        return false;
    }

    // 最简单的指示。tips.好的程序员要学会给自己的函数（以及它的绑定加注释）
    void list()
    {
        int order = 1;
        for (auto& cmd : commands)
        {
            std::cout << order << ". " << cmd.get_pattern() << "\t\tdesc: " << cmd.get_description() << std::endl;
        }
    }

    // 交互模式
    void run_interactive()
    {
        std::string line;
        while (true)
        {
            std::cout << "> ";
            std::getline(std::cin, line);
            if (line == "exit" || line == "quit") break;
            run(line);
        }
    }
};