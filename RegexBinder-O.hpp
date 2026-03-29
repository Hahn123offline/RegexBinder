/**
 * RegexBinder - 基于正则表达式的命令绑定库
 *
 * 特性：
 * - 支持正则表达式匹配命令
 * - 支持普通函数、成员函数、lambda
 * - 支持命令队列和批量执行
 * - 支持函数链（多个函数绑定到同一命令）
 *
 * 示例：
 * @code
 * RegexBinder binder;
 * binder.bind("hello (\\w+)", [](const std::string& name) {
 *     std::cout << "Hello " << name << std::endl;
 * });
 * @endcode
 */

#pragma once

#include<iostream>
#include<functional>
#include<map>
#include<string>
#include<vector>
#include<any>
#include<regex>
#include<tuple>
#include<unordered_map>
#include<algorithm>
#include<cstddef>     

template<typename T>
T parse_argument(const std::string& s) 
{
	// 去掉引用和const，超绝模板大陷阱……
	using RawType = std::remove_cv_t<std::remove_reference_t<T>>;

	if constexpr (std::is_same_v<RawType, int>)
	{
		return std::stoi(s);
	}
	else if constexpr (std::is_same_v<RawType, bool>)
	{
		return s == "true" || s == "1" || s == "yes";
	}
	else if constexpr (std::is_same_v<RawType, double>)
	{
		return std::stod(s);
	}
	else if constexpr (std::is_same_v<RawType, float>)
	{
		return std::stof(s);
	}
	else if constexpr (std::is_same_v<RawType, std::string>)
	{
		return s;
	}
	else
	{
		static_assert(sizeof(T) == 0, "unsupported param type");
	}
}

class Command
{
private:
	std::regex pattern; // 正则表达式
	std::string pattern_str;  // 保存原始字符串，用于显示
	std::string description;  // 可选的注释说明
	std::vector<std::function<void(const std::vector<std::string>&)>> executors;  // 函数链 // 函数对象，负责把vec转换为实际的参数并且完成调用动作

public:

	template<typename Func>
	Command(const std::string& pat, Func&& f, const std::string& desc = "")
		: pattern(pat), pattern_str(pat), description(desc)
	{
		add_executor(std::forward<Func>(f));
	}

	// 添加执行函数到链
	template<typename Func>
	void add_executor(Func&& func) {
		executors.push_back([f = std::forward<Func>(func)](const std::vector<std::string>& args) {
			// 这里需要根据实际情况调用 f
			// 简化：假设都是无参函数
			f();
			});
	}

	// 专门为 std::function 添加
	template<typename Ret, typename... Args>
	void add_executor(std::function<Ret(Args...)> func) {
		executors.push_back([func](const std::vector<std::string>& args)
			{
			if (args.size() != sizeof...(Args))
			{
				throw std::runtime_error("The number of parameters does not match");
			}
			[&]<size_t... I>(std::index_sequence<I...>) {
				func(parse_argument<Args>(args[I])...);
			}(std::index_sequence_for<Args...>{});
			});
	}

	bool match(const std::string& input, std::vector<std::string>& args) const
	{
		std::smatch matches;
		if (std::regex_match(input, matches, pattern))
		{
			for (size_t i = 1; i < matches.size(); i++)
			{
				args.push_back(matches[i].str());
			}
			return true;
		}
		return false;
	}

	void execute(const std::vector<std::string>& args) const
	{
		for (const auto& executor : executors)
		{
			executor(args);  // 顺序执行所有函数
		}
	}

	// 获取信息
	std::string get_pattern() const { return pattern_str; }
	std::string get_description() const { return description; }
	size_t get_executor_count() const { return executors.size(); }

	// 移除最后一个函数
	bool remove_last_executor()
	{
		if (executors.empty()) return false;
		executors.pop_back();
		return true;
	}
};

template<typename... Args>
class CommandBuilder
{
public:
	static Command create(const std::string& pattern, std::function<void(Args...)> func)
	{
		return Command(pattern, [func = std::move(func)](const std::vector<std::string>& args)
			{
				if (args.size() != sizeof...(Args))
				{
					throw std::runtime_error("arguments do not match");
				}

				[&]<size_t... I>(std::index_sequence<I...>)
				{
					func(parse_argument<Args>(args[I])...);
				} (std::index_sequence_for<Args...>{});
			});
	}
};

//// 特化：处理无参函数
//template<>
//class CommandBuilder<>
//{
//public:
//	static Command create(const std::string& pattern,
//		std::function<void()> func)
//	{
//		return Command(pattern,
//			[func = std::move(func)](const std::vector<std::string>&) {
//				func();
//			});
//	}
//};

class RegexBinder
{
private:
	static constexpr size_t MAX_QUEUE_SIZE = 1000;
	static constexpr size_t MAX_EXECUTION_PER_COMMAND = 100;

	std::vector<Command> commands;
	std::vector<std::string> command_queue;
	bool is_executing_queue_ = false;
	std::unordered_map<std::string, int> execution_count_;

	// 版本1：处理无参函数（函数指针版本）
	template<typename Ret>
	void bind_function_impl(const std::string& pattern, Ret(*func)())
	{
		commands.emplace_back(pattern,
			[func](const std::vector<std::string>&) {
				func();  // 无参调用
			});
	}

	// 版本2：处理有参函数（函数指针版本）
	template<typename Ret, typename... Args>
	void bind_function_impl(const std::string& pattern, Ret(*func)(Args...))
	{
		// 使用CommandBuilder
		commands.push_back(CommandBuilder<Args...>::create(pattern,
			std::function<Ret(Args...)>(func)));
	}

	// 版本3：处理无参lambda/函数对象
	template<typename Func>
	auto bind_function_impl(const std::string& pattern, Func&& func,
		std::true_type) -> decltype(func(), void())
	{
		commands.emplace_back(pattern,
			[f = std::forward<Func>(func)](const std::vector<std::string>&) {
				f();  // 无参调用
			});
	}

	// 版本4：处理有参lambda/函数对象
	template<typename Func, typename... Args>
	void bind_function_impl(const std::string& pattern, Func&& func,
		std::false_type)
	{
		// 这里我们需要推断Args...，但无法直接推断
		// 暂时先不实现，用static_assert提示
		static_assert(sizeof...(Args) > 0,
			"For lambdas with parameters, please use the lambda directly in bind or specify type");
	}


public:
	// ========== 基础绑定 ==========

	// 带注释的绑定（函数指针版本）
	template<typename Ret, typename... Args>
	void bind(const std::string& pattern, Ret(*func)(Args...),
		const std::string& description = "") {
		auto it = find_or_create_command(pattern, description);
		it->add_executor(std::function<Ret(Args...)>(func));
	}

	// 带注释的绑定（成员函数版本）
	template<typename T, typename Ret, typename... Args>
	void bind(const std::string& pattern, T* obj, Ret(T::* mem_func)(Args...),
		const std::string& description = "")
	{
		auto wrapper = [obj, mem_func](Args... args) -> Ret {
			return (obj->*mem_func)(args...);
			};
		auto it = find_or_create_command(pattern, description);
		it->add_executor(std::function<Ret(Args...)>(wrapper));
	}

	// 往上是非const成员函数

	// 2. const成员函数
	template<typename T, typename Ret, typename... Args>
	void bind(const std::string& pattern, T* obj, Ret(T::* mem_func)(Args...) const,
		const std::string& description = "")
	{
		auto wrapper = [obj, mem_func](Args... args) -> Ret {
			return (obj->*mem_func)(args...);
			};
		auto it = find_or_create_command(pattern, description);
		it->add_executor(std::function<Ret(Args...)>(wrapper));
	}

	// 3. volatile成员函数（几乎用不到）
	template<typename T, typename Ret, typename... Args>
	void bind(const std::string& pattern, T* obj, Ret(T::* mem_func)(Args...) volatile,
		const std::string& description = "")
	{
		auto wrapper = [obj, mem_func](Args... args) -> Ret {
			return (obj->*mem_func)(args...);
			};
		auto it = find_or_create_command(pattern, description);
		it->add_executor(std::function<Ret(Args...)>(wrapper));
	}

	// 4. const volatile成员函数
	template<typename T, typename Ret, typename... Args>
	void bind(const std::string& pattern, T* obj, Ret(T::* mem_func)(Args...) const volatile,
		const std::string& description = "")
	{
		auto wrapper = [obj, mem_func](Args... args) -> Ret {
			return (obj->*mem_func)(args...);
			};
		auto it = find_or_create_command(pattern, description);
		it->add_executor(std::function<Ret(Args...)>(wrapper));
	}

	// ========== 多函数一次性绑定 ==========

	template<typename Func, typename... Rest>
	void bind_chain(const std::string& pattern, Func&& first, Rest&&... rest) {
		// 先找或创建命令
		auto it = find_or_create_command(pattern);

		// 添加所有函数
		add_functions_to_command(it, std::forward<Func>(first),
			std::forward<Rest>(rest)...);
	}

	// ========== 函数链操作 ==========

	// 追加函数到已有命令
	template<typename Func>
	bool append_func(const std::string& pattern, Func&& func) {
		auto it = find_command_by_name(pattern);
		if (it != commands.end()) {
			it->add_executor(std::forward<Func>(func));
			return true;
		}
		return false;
	}

	// 删除命令的最后一个函数
	bool remove_last_func(const std::string& pattern) {
		auto it = find_command_by_name(pattern);
		if (it != commands.end()) {
			return it->remove_last_executor();
		}
		return false;
	}

	// 删除整个命令
	bool remove_command(const std::string& pattern) {
		auto it = find_command_by_name(pattern);
		if (it != commands.end()) {
			commands.erase(it);
			return true;
		}
		return false;
	}

	// ========== 查询系统 ==========

	// 列出所有命令
	void list_commands(int show_details = 0) const
	{
		std::cout << "\n--- BOUND: (" << commands.size() << ") ---\n";
		for (size_t i = 0; i < commands.size(); i++)
		{
			const auto& cmd = commands[i];
			std::cout << i + 1 << ". " << cmd.get_pattern();

			if (!cmd.get_description().empty())
			{
				std::cout << " \t# " << cmd.get_description();
			}

			if (show_details)
			{
				std::cout << " \t[" << cmd.get_executor_count() << " functions]";
			}
			std::cout << "\n";
		}
		std::cout << "\n";
	}

	// 查询特定命令
	void info(const std::string& pattern) const
	{
		auto it = find_command(pattern);
		if (it != commands.end())
		{
			std::cout << "Cmd: " << it->get_pattern() << "\n";
			if (!it->get_description().empty())
			{
				std::cout << "Info: " << it->get_description() << "\n";
			}
			std::cout << "Bound Funcs: " << it->get_executor_count() << "\n";
		}
		else
		{
			std::cout << "Unknown command: " << pattern << "\n";
		}
	}

	// 搜索命令（支持正则）
	std::vector<std::string> search(const std::string& pattern) const
	{
		std::vector<std::string> result;
		std::regex search_regex(pattern);

		for (const auto& cmd : commands)
		{
			if (std::regex_search(cmd.get_pattern(), search_regex))
			{
				result.push_back(cmd.get_pattern());
			}
		}
		return result;
	}

	bool exe_once(const std::string& input)
	{
		for (const auto& cmd : commands)
		{
			std::vector<std::string> args;
			if (cmd.match(input, args))
			{
				cmd.execute(args);
				return true;
			}
		}
		std::cout << "Unknown command" << std::endl;
		return false;
	}

	bool exe_terminal()
	{
		std::cout << "> ";
		std::string input;
		std::getline(std::cin, input);

		return exe_once(input);
	}

	void queue(const std::string& cmd)
	{
		if (command_queue.size() >= MAX_QUEUE_SIZE)
		{
			std::cout << "The queue is full" << cmd << "\n";
			return;
		}

		if (is_executing_queue_ && cmd.find("queue") == 0)
		{
			std::cout << "The queue is being executed, cannot add a new command" << cmd << "\n";
			return;
		}

		command_queue.push_back(cmd);
	}

	void exe_queue()
	{
		if (is_executing_queue_)
		{
			std::cout << "Commands of the current queue is being executed\n";
			return;
		}

		is_executing_queue_ = true;
		execution_count_.clear();

		std::cout << "Queue size = " << command_queue.size() << ", execute:\n";

		auto queue_copy = command_queue;
		command_queue.clear();

		for (const auto& input : queue_copy)
		{
			bool executed = false;
			for (const auto& cmd : commands)
			{
				if (execution_count_[input] > MAX_EXECUTION_PER_COMMAND)
				{
					std::cout << "Command \"" << input << "\" has been executed too many times, stop execution\n";
					break;
				}


				std::vector<std::string> args;
				if (cmd.match(input, args))
				{
					cmd.execute(args);
					executed = true;
					break;
				}
			}
			if (!executed)
			{
				std::cout << "Fail to execute / Unknown command: " << input << "\n";
			}
		}
		command_queue.clear();
		is_executing_queue_ = false;
		std::cout << "Execution finished\n";
	}

	private:
    // 辅助函数：查找命令
    std::vector<Command>::iterator find_command_by_name(const std::string& pattern) {
        return std::find_if(commands.begin(), commands.end(),
            [&pattern](const Command& cmd) {
                return cmd.get_pattern() == pattern;
            });
    }

    std::vector<Command>::const_iterator find_command(const std::string& pattern) const {
        return std::find_if(commands.begin(), commands.end(),
            [&pattern](const Command& cmd) {
                return cmd.get_pattern() == pattern;
            });
    }

    // 辅助函数：查找或创建命令
    std::vector<Command>::iterator find_or_create_command(
        const std::string& pattern, const std::string& description = "") {
        auto it = find_command_by_name(pattern);
        if (it == commands.end()) {
            commands.emplace_back(pattern, description);
            it = commands.end() - 1;
        }
        return it;
    }

    // 辅助函数：批量添加函数到命令
    template<typename Func>
    void add_functions_to_command(std::vector<Command>::iterator it, Func&& func) {
        it->add_executor(std::forward<Func>(func));
    }

    template<typename Func, typename... Rest>
    void add_functions_to_command(std::vector<Command>::iterator it, 
                                   Func&& first, Rest&&... rest) {
        it->add_executor(std::forward<Func>(first));
        add_functions_to_command(it, std::forward<Rest>(rest)...);
    }
};