# FIRFilter

FIR-фильтрация сигнала.

Коэффициенты получаются от другого модуля (BandPassCompute, RRCCompute) через VirtualTX/VirtualRX. Поддерживает вещественные и комплексные коэффициенты.

## Параметры

| Параметр | Тип | Описание |
|---|---|---|
| `coefficients data tag` | string | Тег для получения коэффициентов |
| `filter order` | int | Порядок фильтра (нечётное число) |
| `coefficients type` | string | Тип: `auto`, `real`, `complex` |
| `log energy` | bool | Логировать энергию (по умолчанию true) |
