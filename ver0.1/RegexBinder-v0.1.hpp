#pragma once

// v0.1

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


// 特征匹配

template<typename Func>
struct function_traits; // 主模板，不实现，全空仅用于特化

// 特化：处理普通函数指针
// void add(int a, int b) --> void(*)(int, int)
template<typename Ret, typename... Args>
struct function_traits<Ret(*)(Args...)>
{
    using return_type = Ret; // 返回值类型（函数指针的）
    using args_tuple = std::tuple<Args...>; // 打包函数的参数为元组
    static constexpr size_t arity = sizeof...(Args); // 参数个数

    template<size_t I> // 辅助功能：获得第I个参数的类型。
    using arg_type = std::tuple_element_t<I, args_tuple>; //arg_type<0> 为第一个参数类型
};

// 特化：处理lambda const
// lambda是一个有operator()的类，[](int a, int b){}的operator()类型为void(int, double) const
template<typename Ret, typename Class, typename... Args>
struct function_traits<Ret(Class::*)(Args...) const>
{
    using return_type = Ret;
    using args_tuple = std::tuple<Args...>;
    static constexpr size_t arity = sizeof...(Args);

    template<size_t I>
    using arg_type = std::tuple_element_t<I, args_tuple>;
};

// 特化：处理 lambda（非 const） 
// 有些 lambda 是 mutable 的，operator() 没有 const
template<typename Ret, typename Class, typename... Args>
struct function_traits<Ret(Class::*)(Args...)>
{
    using return_type = Ret;
    using args_tuple = std::tuple<Args...>;
    static constexpr size_t arity = sizeof...(Args);

    template<size_t I>
    using arg_type = std::tuple_element_t<I, args_tuple>; // 里面好像都差不多？！？就是接受+统一打包吗
};

// 特化：任意可调用对象
// 对于 lambda 对象，取其 operator() 的类型，再套用上面的特化。相当于这是前置步骤
// decltype(&Func::operator()) 得到成员函数指针类型
// 怎么长得像一个构造函数？
template<typename Func>
struct function_traits : function_traits<decltype(&Func::operator())> {};


class Command // 指令构成
{
private:
	std::regex pattern;
	std::function<void(const std::vector<std::string>&)> handler;
    std::string description;
    std::string pattern_str;
    std::string name = "";

public:
	Command(const std::string& pat, std::function<void(const std::vector<std::string>&)> h
    , const std::string& desc = "")
		: pattern(pat), handler(h), description(desc), pattern_str(pat)
        // 注意：这里什么都没执行！↑，给handler传入h只是让它知道规则
       // 只是把正则模式和如何执行的指令保存起来
    {
        // 构造时提取命令名！希望管用
        size_t space = pat.find(' ');
        if (space != std::string::npos) {
            name = pat.substr(0, space);
        }
        else {
            // 处理特殊模式如 "help(?:\\s+(.+))?" 这种初始指令有特殊模式……
            if (pat.find("help") == 0) name = "help";
            else if (pat.find("list") == 0) name = "list";
            else if (pat.find("clear") == 0) name = "clear";
            else if (pat.find("exit") == 0) name = "exit";
            else name = pat;  // fallback
        }
    }
       

    const std::string& get_description() const { return description; }
    const std::string& get_pattern() const { return pattern_str; } // 记录完整正则表达式
    const std::string& get_name() const { return name; } // 记录命令名称

	bool matches(const std::string& input, std::vector<std::string>& args) const
	{
		std::smatch matches; // 容器
		if (std::regex_match(input, matches, pattern))
            // 实际输入+接受输入数据+解析模板↑
		{
			for (size_t i = 1; i < matches.size(); i++)
			{
				args.push_back(matches[i].str());
                // 成功！给参数传入解析到的数据，真好用啊
			}
			// handler(args); // 这个参数是实实在在的，执行！！// 解耦，不执行
			return true;
		}
		return false;
	}

    bool matches_regex(const std::string& input) const // 纯查找
    {
        return std::regex_match(input, pattern);
    }

    bool matches_name(const std::string& input) const // 纯查找，看名字
    {
        return input == name;
    }

    void execute(const std::vector<std::string>& args) const { handler(args); }
};

class RegexBinder
{
private:
    // 防止外部创建
    RegexBinder()
    {
        register_builtins(); // 注册内置功能，后面可以有更多！……
    }

    void register_builtins()
    {
        // 内置指令也是普通的 bind
        bind("list", [this]() { list(); }, "list all commands(no desc)");
        bind("help(?:\\s+(.+))?",
            [this](std::string cmd) { help(cmd); },
            "show help (help [cmd])");
        bind("clear", []() { system("clear"); }, "clear scr");
        bind("exit", []() { exit(0); }, "exit program loop");
    }

    // 私有拷贝构造和赋值（防止复制）
    RegexBinder(const RegexBinder&) = delete;
    RegexBinder& operator=(const RegexBinder&) = delete;

    // 内部辅助函数：根据参数个数生成包装器
    template<typename Func, size_t... I>
    void bind_impl(const std::string& pattern,
        Func&& func,                    // 用户传入的函数/lambda
        const std::string& desc,
        std::index_sequence<I...>) // 比如 <0,1,2> 表示有3个参数
    {

        using traits = function_traits<std::decay_t<Func>>;  // 获取函数特征，用decay_t去除引用const, volatile得到原始类型。const int& --decay--> int
        // traits::arity 参数个数
        // traits::arg_type<0> 第一个参数类型
        // traits::arg_type<1> 第二个参数类型

        commands.emplace_back(pattern,
            [func = std::forward<Func>(func)](const std::vector<std::string>& args)
            {
                // 检查参数数量是否匹配
                if (args.size() != sizeof...(I)) {
                    std::cout << "参数数量错误：需要 " << sizeof...(I)
                        << " 个，实际 " << args.size() << " 个\n";
                    return; // 在这里return就不emplace了
                }
                // 核心：展开参数包
                // 对于 I = 0,1,2...
                // parse_arg<typename traits::arg_type<I>>(args[I])...
                // 会变成：parse_arg<int>(args[0]), parse_arg<double>(args[1]), ...
                func(parse_arg<typename traits::template arg_type<I>>(args[I])...);
            }, desc);
    }

	std::vector<Command> commands; // 小小一个命令储存

public:
    static RegexBinder& global() // 唯一访问接口
    {
        static RegexBinder instance;
        return instance;
    }

    //帮助，这里用到遍历。记得后期在这里进行查找优化

    // 计算两个字符串的差异程度（越小越相似）
    int edit_distance(const std::string& a, const std::string& b)
    {
        int cost = 0;
        // 简化版：直接比较字符差异
        for (size_t i = 0; i < std::min(a.size(), b.size()); i++)
        {
            if (a[i] != b[i]) cost++;
        }
        cost += abs((int)a.size() - (int)b.size());
        return cost;
    }

    // 找到最相似的命令
    std::string suggest(const std::string& wrong_cmd)
    {
        int best_score = 999;
        std::string best_match;

        for (auto& cmd : commands)
        {
            std::string name = cmd.get_name(); // 专门用于匹配的无正则名字！

            int score = edit_distance(wrong_cmd, name);
            if (score < best_score && score <= 3)
            {  // 差异不超过3个字符
                best_score = score;
                best_match = name;
            }
        }
        return best_match;
    }

    void help(const std::string& cmd = "")
    {
        if (cmd.empty())
        {
            // 显示所有命令的简短帮助
            std::cout << "\nbound：\n";
            for (size_t i = 0; i < commands.size(); i++)
            {
                std::cout << "  " << commands[i].get_pattern()
                    << "\n    " << commands[i].get_description() << "\n";
            }
        }
        else
        {
            // 显示特定命令的详细帮助
            if (find_command_by_name(cmd) != -1)
            {
                Command command = commands[find_command_by_name(cmd)];
                std::cout << "CMD name：" << command.get_name() << "\n";
                std::cout << "Regex：" << command.get_pattern() << "\n";
                std::cout << "Desc: " << command.get_description() << "\n";
                // 可以再加：参数说明、示例等
            }
            else
            {
                std::string suggestion = suggest(cmd);
                if (!suggestion.empty()) {
                    std::cout << "Unknown command: " << cmd
                        << "\nDid you mean: " << suggestion << "?\n";
                }
                else {
                    std::cout << "Unknown command: " << cmd << "\n";
                }
            }
        }
    }

    /*  万能版本，你再也不需要下面那些了。针对所有全局函数  */
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

    /*  针对成员函数  */

    // 包装器函数，辅助
    template<typename T, typename Ret, typename... Args, size_t... I>
    void bind_mem_impl(const std::string& pattern,
        T* obj,
        Ret(T::* mem_func)(Args...),
        const std::string& desc,
        std::index_sequence<I...>)
    {
        commands.emplace_back(pattern,
            [obj, mem_func](const std::vector<std::string>& args) {
                if (args.size() != sizeof...(I)) {
                    std::cout << "参数数量错误\n";
                    return;
                }
                // 调用成员函数
                (obj->*mem_func)(parse_arg<Args>(args[I])...);
            }, desc);
    }

    // 非 const 函数
    template<typename T, typename Ret, typename... Args>
    void bind(const std::string& pattern,
        T* obj,                              // 对象指针
        Ret(T::* mem_func)(Args...),         // 成员函数指针
        const std::string& desc = "")
    {
        // 复用 function_traits 提取参数类型
        using traits = function_traits<Ret(T::*)(Args...)>;
        constexpr size_t arity = traits::arity;

        // 生成包装器
        bind_mem_impl(pattern, obj, mem_func, desc,
            std::make_index_sequence<arity>{});
    }

    // const 函数
    template<typename T, typename Ret, typename... Args>
    void bind(const std::string& pattern,
        T* obj,
        Ret(T::* mem_func)(Args...) const,  // 注意 const
        const std::string& desc = "")
    {
        using traits = function_traits<Ret(T::*)(Args...) const>;
        constexpr size_t arity = traits::arity;

        bind_mem_impl(pattern, obj, mem_func, desc,
            std::make_index_sequence<arity>{});
    }

    // 查找命令（返回索引）
    int find_command_by_regex(const std::string& input, std::vector<std::string>& args)
    {
        for (size_t i = 0; i < commands.size(); i++)
        {
            if (commands[i].matches(input, args))
            {
                return i;
            }
        }
        return -1;
    }

    // 纯查找（不提取参数）
    int find_command_by_name(const std::string& input)
    {
        for (size_t i = 0; i < commands.size(); i++)
        {
            if (commands[i].get_name() == input) return i;
        }
        return -1;
    }

    // 触发一条指令
    bool run(const std::string& input)
    {
        std::vector<std::string> args;
        int idx = find_command_by_regex(input, args);
        if (idx != -1)
        {
            commands[idx].execute(args);
            return true;
        }
        return false;
    }

    // 最简单的指示。tips.好的程序员要学会给自己的函数（以及它的绑定加注释）
    void list()
    {
        int order = 1;
        for (auto& cmd : commands)
        {
            std::cout << order++ << ". " << cmd.get_pattern() << std::endl;
        }
    }

    // 交互模式，阻塞
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

    int levenshtein(const std::string& s1, const std::string& s2) // 指令相似度匹配
    {
        size_t n = s1.size(), m = s2.size();
        std::vector<std::vector<int>> dp(n + 1, std::vector<int>(m + 1));

        for (size_t i = 0; i <= n; i++) dp[i][0] = i;
        for (size_t j = 0; j <= m; j++) dp[0][j] = j;

        for (size_t i = 1; i <= n; i++)
        {
            for (size_t j = 1; j <= m; j++)
            {
                if (s1[i - 1] == s2[j - 1])
                {
                    dp[i][j] = dp[i - 1][j - 1];
                }
                else
                {
                    dp[i][j] = std::min({ dp[i - 1][j], dp[i][j - 1], dp[i - 1][j - 1] }) + 1;
                }
            }
        }
        return dp[n][m];
    }

    void clear() { commands.clear(); }
};