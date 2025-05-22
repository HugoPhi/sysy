- Sys-Y Code: 

```c
int a[10][10];
int main(){
    return 0;
}
```

- IR code: 

```text
void global()
    0: alloc a, 100          // 分配10x10的整型数组（共100个int空间）
    1: return null           // 全局初始化函数返回
end


int main()
    0: call t0, global()     // 调用全局初始化函数
    1: return 0              // 主函数返回0
end


GVT:
    a int[10][10]            // 全局数组a的声明（类型为10x10的整型数组）
```