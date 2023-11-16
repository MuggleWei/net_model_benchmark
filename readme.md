# network model benchmark

## 运行
* 限定 CPU 的频率, 例如
```
sudo cpupower frequency-set -g performance
sudo cpupower frequency-set --max 3.2GHz --min 3.2GHz
```
* 运行 `bin/gen_numbers` 产生需要一定计算量的数
* 运行 `bin/gen_orders` 将上一步产生的数填入订单中
* 运行 `bin/upstream` 启动上手服务
* 运行 `bin/server_multiplex` 或 `bin/server_channel` 启动被测服务
* 将 `run_client.sh` 拷贝至 `build` 目录中并运行, 生成报告

通常情况下, 还可以选择对服务进行绑核操作, 例如
```
taskset -c 0 ./bin/upstream
taskset -c 1,2,3,4 ./bin/server_multiplex
```
