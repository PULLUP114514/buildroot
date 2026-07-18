# epass 电池电量接入指南

两个家族最终暴露**同一套用户态接口**：`/sys/class/power_supply/<名字>/` 下的
`capacity`（0-100）、`voltage_now`（µV）、`status`（Charging/Discharging）、`present`。
app 只认 power_supply class，不需要知道底下是哪条路线。

唯一的能力差异用 `charging_capacity_reliable`（只读，0/1）表达：充电时
`capacity` 还可不可信。gab 路线暴露此文件且恒为 0——capacity 是 OCV 查表，
充电时采样轨被充电器拉高（判 charging 靠的就是这个），查表结果必然钳在
100% 附近。axp20x 是真电量计，**没有这个文件，文件不存在视为 1**。

| 家族 | 硬件 | 驱动链路 | psy 名字 |
|---|---|---|---|
| epass_p.dtsi | AXP209 PMIC | axp20x_battery（真库仑计） | `axp20x-battery` |
| epass.dtsi | 分压 → PA0 (TP aux ADC) | sun4i-gpadc → iio-rescale → generic-adc-battery | `adc-battery` |

epass_p 家族接 AXP209 后开 `CONFIG_BATTERY_AXP20X` 即可，不在本文范围。
下面讲无 PMIC 路线怎么给**新板型/新电池**接入。

## 驱动链路（无 PMIC 路线）

```
电池 ─ 分压电阻 ─ PA0
                  │ rtp 节点 (sun4i-gpadc-iio, 12bit, 满量程按 3.0V 折算)
                  │   → in_voltage0_raw × scale = ADC 脚电压(mV)
                  │ voltage-divider 节点 (iio-rescale)
                  │   → × full-ohms/output-ohms = 电池电压
                  │ adc-battery 节点 (generic-adc-battery)
                  │   → voltage_now(µV)、OCV 查表→capacity、阈值→status
                  ▼
        /sys/class/power_supply/adc-battery/
```

相关 patch（`patch/linux/`）：
- `0000-f1c100s-gpadc-regs.patch`：F1C 的 TP_CTRL1 位域与 A10 不同
- `0020-sun4i-gpadc-f1c-stability.patch`：通道使能位、FIFO flush 时机、
  常驻 aux ADC 模式、TACQ=1023/122Hz、scale 用 FRACTIONAL、MFD 子设备
  借 rtp 的 of_node（io-channels 能解析的前提）
- `0021-generic-adc-battery-v6.4-backport.patch`：DT 版 gab + 本地扩展
  （capacity/charging 阈值/present，上游 v6.4 没有这些）

## 接入步骤

### 1. 硬件前提

- 电池（或系统供电轨）经分压电阻接 PA0，分压后电压在满电时 < 2V 左右
  （满量程 3V 折算，留余量）。
- ADC 采样窗已放宽到 4ms（TACQ=1023），高阻分压（百 kΩ 级）可用；
  分压点并一个 100nF 电容会更稳。
- 判 charging 的前提：充电器插入时能把采样点拉到电池开路电压达不到的
  值（采样轨接系统电源轨/充电器输出侧才成立）。做不到就删
  `charging-threshold-microvolt`，status 恒报 Discharging。
- 以后硬件加了充电指示脚，gab 支持 `charged-gpios`，dts 加一行即可。

### 2. dts 节点

模板（现 `devicetree/linux/base/epass.dtsi`，新板型抄这个改数值）：

```dts
bat: battery {
    compatible = "simple-battery";
    voltage-min-design-microvolt = <3300000>;
    voltage-max-design-microvolt = <4200000>;
    ocv-capacity-celsius = <20>;
    ocv-capacity-table-0 = <4200000 100>, <4050000 75>,
                           <3900000 50>, <3750000 25>,
                           <3600000 0>;          /* 电压µV 电量%，降序 */
};

vbat_sense: voltage-divider {
    compatible = "voltage-divider";
    io-channels = <&rtp 0>;      /* 0 = PA0 = aux ADC chan0 */
    full-ohms = <2294>;          /* 分压比 = full/output，见标定 */
    output-ohms = <1000>;
    #io-channel-cells = <1>;
};

adc_bat: adc-battery {
    compatible = "adc-battery";
    io-channels = <&vbat_sense 0>;
    io-channel-names = "voltage";
    monitored-battery = <&bat>;
    charging-threshold-microvolt = <4370000>;
    /* 可选，默认阈值/100: charging-hysteresis-microvolt = <50000>; */
};
```

内核配置：`CONFIG_GENERIC_ADC_BATTERY=y` + `CONFIG_IIO_RESCALE=y`（已在
linux.defconfig）。

### 3. 标定分压比

阻值抄原理图最好（full = R上+R下，output = R下，单位随意只要比率对）；
没有原理图就两点法实测：

1. 电池较满和较空各测一组：万用表量电池端电压 V_bat，同时
   `cat /sys/bus/iio/devices/iio:device0/in_voltage0_raw` 记下 raw。
2. 每组算 k = V_bat(mV) / (raw × 3000 / 4096)，两组 k 应基本一致
   （不一致说明分压受负载影响，检查硬件）。
3. `full-ohms = k × 1000`，`output-ohms = 1000`。

ADC 实际满量程即使不是 3.0V，误差是纯比例的，会被 k 吸收，不用改驱动。

标定后验证：`cat voltage_now` 应与万用表读数吻合（±2% 内算好）。

> 当前 epass.dtsi 里的 2294/1000 是由旧 app 阈值（full=raw2500≈4.2V）
> 反推的**临时值**，未实测。

### 4. 标定 OCV 表

`capacity` 由 `voltage_now` 查 `ocv-capacity-table-0` 线性插值得出，
表必须按电压降序，两端自动钳位（高于表头=100，低于表尾=0）。

- 快速版：照电芯 datasheet 的 OCV 曲线取 8~10 个点。
- 认真版：满电后静置断充，间隔放电+静置记录电压/已放容量。
- 凑合版（现状）：沿用 app 旧图标分档换算的 5 点线性表。

注意这是端电压不是真 OCV，带载时偏低、显示会保守，对电量图标够用。
换不同容量/化学体系的电芯只需要改这张表和 min/max design。

### 5. 标定 charging 阈值

1. 不插充电器，满电时记 `voltage_now` 的最大值 V_max。
2. 插上充电器，各电量段记 `voltage_now`，取最小值 V_chg。
3. 阈值取两者中间偏上：`charging-threshold-microvolt ≈ (V_max+V_chg)/2`，
   且要求 V_chg - V_max 明显大于噪声峰峰值 + 迟滞。
4. 插拔充电器各几次，确认 `status` 干脆利落地翻转、阈值附近不抖。

### 6. 验证清单

```sh
grep . /sys/class/power_supply/adc-battery/uevent   # 字段齐全
# 读数稳定性:
for i in $(seq 100); do cat /sys/class/power_supply/adc-battery/voltage_now; done \
  | awk '{s+=$1;ss+=$1*$1;n++} END{m=s/n; print "mean",m,"std",sqrt(ss/n-m*m)}'
```

与 epass_p 板对比 `axp20x-battery` 的 uevent，确认 app 用到的字段
（capacity/status/voltage_now/present）两边都有。

## app 侧约定

- 遍历 `/sys/class/power_supply/*/type`，取 `Battery` 的那个目录，
  不要写死名字（两家族名字不同）。
- 画电量读 `capacity`，画闪电读 `status`，别再读 iio raw。
- 启动时读一次 `charging_capacity_reliable`（不存在=1）。为 0 时，
  `status == Charging` 期间不要更新电量格数（显示闪电+最后一次
  放电时的电量，或只显示闪电），拔充电器后恢复正常刷新。
- 每次读 `capacity`/`voltage_now` 会触发一次 ADC 转换，阻塞
  0.13~0.26s——不要放在 LVGL 线程里同步读。

## 已知坑

- 5.4 的 iio-rescale 只认 FRACTIONAL/INT/FRACTIONAL_LOG2 的 scale，
  上游 sun4i-gpadc 的 INT_PLUS_NANO 会让整条链 -EOPNOTSUPP（0020 已改）。
- io-channels 指向 rtp 能解析，靠的是 0020 里给 MFD 子设备借 of_node
  + rtp 节点的 `#io-channel-cells`，升级内核时这两处不能丢。
- 改了 board defconfig 的 `BR2_LINUX_KERNEL_CUSTOM_DTS_PATH` 后，
  顶层 .config 不会自动同步，dtb 会报 include 找不到。
- rtp 的 aux ADC 和电阻触摸共用 IP：现在 rtp 只当 ADC 用（PA0 一根脚）；
  哪天真要接电阻触摸屏，两个功能会打架，需要另行安排。
