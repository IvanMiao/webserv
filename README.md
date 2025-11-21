# webserv
A Nginx-like webserv in C++

## TODO

1. how to write a Nginx conf file? how many keywords/blocks... should we manage?

2. how to initialize a server

3. choose `poll` or `epoll`?

4. a Logger? 

## Code Spec

1. 每个 header 文件有 #define Guard
2. 每个文件(.cpp, .hpp)有 namespace 包裹， 主要的namespace为 **`wsv`**

### Names

1. class name: UpperCamelCase format
2. 私有变量和私有函数: 一律以 `_` 开头命名
3. 函数名： `myFunction()` (第一个单词小写，之后每一个新单词首字母大写)
4. 变量名： `snake_case` （全部小写，单词间用下划线分隔）


```c++
namespace wsv{
class Name
{
private:
	std::string _name;

public:
	void	getName();
};
} // namespace wsv
```