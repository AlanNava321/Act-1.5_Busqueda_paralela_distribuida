#include <iostream>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <omp.h>

#if defined(_WIN32)
    #include <winsock2.h>
#else
    #include <unistd.h>
#endif

// Obtiene el nombre de la maquina local
void get_hostname(char* buffer, size_t size) {
#if defined(_WIN32)
    DWORD len = size;
    GetComputerNameA(buffer, &len);
#else
    gethostname(buffer, size);
#endif
}

class LocalLogger {
private:
    std::ofstream log_file;
    std::string hostname;

public:
    void init() {
        char host_buf[256];
        get_hostname(host_buf, sizeof(host_buf));
        hostname = std::string(host_buf);

        std::stringstream ss;
        ss << "log_equipo_" << hostname << "_local.txt";
        log_file.open(ss.str().c_str(), std::ios::out | std::ios::app);

        log_file << "====================================================\n";
        log_file << "[INICIO DE SESION LOCAL]\n";
        log_file << "Equipo (PC): " << hostname << "\n";
        log_file << "Hilos OpenMP disponibles: " << omp_get_max_threads() << "\n";
        log_file << "====================================================\n" << std::flush;
    }

    void log(int thread_id, long long start_idx, long long end_idx, const std::string& algoritmo, const std::string& mensaje) {
        if (log_file.is_open()) {
            log_file << "[Equipo: " << hostname << "] "
                     << "[Nodo/Proceso: 0 (Local)] "
                     << "[Hilo OpenMP: " << thread_id << "] "
                     << "[Bloque: " << start_idx << " - " << end_idx << "] "
                     << "[Algoritmo: " << algoritmo << "] "
                     << "[Estado: " << mensaje << "]\n" << std::flush;
        }
    }

    void log_plain(const std::string& msg) {
        if (log_file.is_open()) {
            log_file << msg << "\n" << std::flush;
        }
    }

    ~LocalLogger() {
        if (log_file.is_open()) {
            log_file << "[FIN DE SESION LOCAL]\n\n";
            log_file.close();
        }
    }
};

LocalLogger logger;

// Ordenamiento paralelo

void merge(int* arr, long long l, long long m, long long r) {
    long long n1 = m - l + 1;
    long long n2 = r - m;

    // Memoria dinamica con punteros en C++
    int* L = new int[n1];
    int* R = new int[n2];

    for (long long i = 0; i < n1; i++) L[i] = arr[l + i];
    for (long long j = 0; j < n2; j++) R[j] = arr[m + 1 + j];

    long long i = 0, j = 0, k = l;
    while (i < n1 && j < n2) {
        if (L[i] <= R[j]) arr[k++] = L[i++];
        else arr[k++] = R[j++];
    }
    while (i < n1) arr[k++] = L[i++];
    while (j < n2) arr[k++] = R[j++];

    delete[] L;
    delete[] R;
}

void parallel_merge_sort_rec(int* arr, long long l, long long r, int depth) {
    if (l < r) {
        long long m = l + (r - l) / 2;
        if (depth < 4) {
            #pragma omp task shared(arr)
            parallel_merge_sort_rec(arr, l, m, depth + 1);
            #pragma omp task shared(arr)
            parallel_merge_sort_rec(arr, m + 1, r, depth + 1);
            #pragma omp taskwait
        } else {
            parallel_merge_sort_rec(arr, l, m, depth + 1);
            parallel_merge_sort_rec(arr, m + 1, r, depth + 1);
        }
        merge(arr, l, m, r);
    }
}

void parallel_merge_sort(int* arr, long long n) {
    #pragma omp parallel
    {
        #pragma omp single
        {
            parallel_merge_sort_rec(arr, 0, n - 1, 0);
        }
    }
}

// Busqueda

// Busqueda secuencial paralela
long long busqueda_secuencial_local(const int* arr, long long n, int target, bool verbose) {
    long long found_idx = -1;

    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        int total_threads = omp_get_num_threads();
        long long items_per_thread = n / total_threads;
        long long start = tid * items_per_thread;
        long long end = (tid == total_threads - 1) ? n : start + items_per_thread;

        for (long long i = start; i < end; ++i) {
            if (arr[i] == target) {
                #pragma omp critical
                {
                    if (found_idx == -1 || i < found_idx) {
                        found_idx = i;
                    }
                }
                if (verbose) {
                    std::stringstream ss;
                    ss << "Elemento Encontrado en Indice Global: " << i;
                    logger.log(tid, start, end, "Busqueda Secuencial", ss.str());
                }
                break;
            }
        }

        if (verbose && found_idx == -1) {
            logger.log(tid, start, end, "Busqueda Secuencial", "Elemento NO Encontrado");
        }
    }

    return found_idx;
}

// Busqueda binaria paralela
long long busqueda_binaria_local(const int* arr, long long n, int target, bool verbose) {
    if (n == 0 || target < arr[0] || target > arr[n - 1]) {
        if (verbose) {
            logger.log(0, 0, n, "Busqueda Binaria", "Descartado: fuera de rango del arreglo");
        }
        return -1;
    }

    long long found_idx = -1;

    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        int total_threads = omp_get_num_threads();
        long long items_per_thread = n / total_threads;
        long long start = tid * items_per_thread;
        long long end = (tid == total_threads - 1) ? n : start + items_per_thread;

        if (start < end && target >= arr[start] && target <= arr[end - 1]) {
            long long low = start;
            long long high = end - 1;
            long long local_found = -1;

            while (low <= high) {
                long long mid = low + (high - low) / 2;
                if (arr[mid] == target) {
                    local_found = mid;
                    break;
                }
                if (arr[mid] < target) low = mid + 1;
                else high = mid - 1;
            }

            if (local_found != -1) {
                #pragma omp critical
                {
                    if (found_idx == -1 || local_found < found_idx) {
                        found_idx = local_found;
                    }
                }
                if (verbose) {
                    std::stringstream ss;
                    ss << "Elemento encontrado en: " << local_found;
                    logger.log(tid, start, end, "Busqueda binaria", ss.str());
                }
            } else if (verbose) {
                logger.log(tid, start, end, "Busqueda binaria", "Elemento NO Encontrado en sub-intervalo");
            }
        } else if (verbose) {
            logger.log(tid, start, end, "Busqueda binaria", "Descartado: fuera de rango del hilo");
        }
    }

    return found_idx;
}


int main() {
    logger.init();

    int* array_dinamico = NULL;
    long long N = 0;
    bool is_sorted = false;
    int option = 0;

    while (true) {
        std::cout << "\n-----------------------------------------------\n";
        std::cout << "  BUSQUEDA PARALELA\n";
        std::cout << "-----------------------------------------------\n";
        std::cout << "1. Generar arreglo dinamico\n";
        std::cout << "2. Ordenar arreglo\n";
        std::cout << "3. Busqueda secuencial\n";
        std::cout << "4. Busqueda binaria \n";
        std::cout << "5. Salir\n";
        std::cout << "Seleccione una opcion: ";
        std::cin >> option;

        if (option == 5) break;

        switch (option) {
            case 1: {
                std::cout << "Ingrese el tamano del arreglo (N): ";
                std::cin >> N;

                if (array_dinamico != NULL) delete[] array_dinamico;
                array_dinamico = new int[N];

                srand(static_cast<unsigned int>(time(NULL)));
                for (long long i = 0; i < N; ++i) {
                    array_dinamico[i] = rand() % (N * 2);
                }
                is_sorted = false;
                std::cout << "-> Arreglo dinamico de tamano " << N << " generado\n";

                std::stringstream ss;
                ss << "Arreglo generado. Tamano: " << N;
                logger.log_plain(ss.str());
                break;
            }

            case 2: {
                if (N <= 0 || array_dinamico == NULL) {
                    std::cout << "[ERROR] Primero debe generar un arreglo (Opcion 1).\n";
                    break;
                }

                std::cout << "-> Ordenando arreglo\n";
                double t_start = omp_get_wtime();
                parallel_merge_sort(array_dinamico, N);
                double t_end = omp_get_wtime();

                is_sorted = true;
                double elapsed = t_end - t_start;
                std::cout << "-> Arreglo ordenado con exito en " << elapsed << " segundos\n";

                std::stringstream ss;
                ss << "Ordenamiento completado en " << elapsed << " s.";
                logger.log_plain(ss.str());
                break;
            }

            case 3:
            case 4: {
                if (N <= 0 || array_dinamico == NULL) {
                    std::cout << "[ERROR] Primero debe generar un arreglo (Opcion 1).\n";
                    break;
                }

                if (option == 4 && !is_sorted) {
                    std::cout << "El arreglo no esta ordenado\n";
                }

                int target = 0;
                std::cout << "Ingrese el valor a buscar (target): ";
                std::cin >> target;

                bool verbose = (N <= 100);

                std::cout << "\n--- Ejecutando busqueda paralela---\n";
                double t_start = omp_get_wtime();
                long long idx = -1;

                if (option == 3) {
                    idx = busqueda_secuencial_local(array_dinamico, N, target, verbose);
                } else {
                    idx = busqueda_binaria_local(array_dinamico, N, target, verbose);
                }
                double t_end = omp_get_wtime();

                std::cout << "Resultado: "
                          << (idx != -1 ? "Encontrado en indice global " + std::to_string(idx) : "NO Encontrado")
                          << " | Tiempo: " << (t_end - t_start) << " s\n";
                break;
            }

            default:
                std::cout << "Opcion no valida. Intente de nuevo.\n";
                break;
        }
    }

    if (array_dinamico != NULL) delete[] array_dinamico;
    return 0;
}
