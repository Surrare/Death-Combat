// Ноутбуки с двумя видеокартами: просим драйвер запустить игру на дискретной (docs/ARCHITECTURE.md §8.1).
// Это единственные изменяемые глобальные переменные в проекте: драйверы ищут их по имени в .exe,
// поэтому файл обязан быть в исполняемом файле, а не в статической библиотеке.
extern "C" {
__declspec(dllexport) unsigned long NvOptimusEnablement = 1;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
