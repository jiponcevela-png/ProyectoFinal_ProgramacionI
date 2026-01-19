#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ZONAS 5
#define DIAS 30
#define CONT 4

// Índices de contaminantes
#define PM25 0
#define NO2  1
#define SO2  2
#define CO2  3

// Archivos
#define ARCHIVO_DATOS "datos_contaminacion_qto.csv"
#define ARCHIVO_REPORTE "reporte_contaminacion.txt"

// Límites de referencia (valores de ejemplo, personalizables por docente)
// Unidades típicas: PM2.5/NO2/SO2 en ug/m3, CO2 en ppm (solo referencial)
const float LIMITES[CONT] = { 15.0f, 40.0f, 40.0f, 1000.0f };

const char *NOMBRES_CONT[CONT] = { "PM2.5", "NO2", "SO2", "CO2" };

typedef struct {
    float temp;    // °C
    float viento;  // m/s
    float humedad; // %
} Clima;

typedef struct {
    char nombre[40];
    float hist[CONT][DIAS]; // 30 días por contaminante
    Clima climaActual;

    // Calculados
    float promedio30[CONT];
    float actual[CONT];
    float pred24h[CONT];
} Zona;

// ---------- Utilidades ----------
void pausar() {
    printf("\nPresiona ENTER para continuar...");
    getchar();
}

int leerEnteroSeguro(const char *msg, int min, int max) {
    char linea[64];
    int v;
    while (1) {
        printf("%s", msg);
        if (!fgets(linea, sizeof(linea), stdin)) continue;
        if (sscanf(linea, "%d", &v) == 1 && v >= min && v <= max) return v;
        printf("Entrada invalida. Intenta de nuevo (%d a %d).\n", min, max);
    }
}

float leerFloatSeguro(const char *msg, float min, float max) {
    char linea[64];
    float v;
    while (1) {
        printf("%s", msg);
        if (!fgets(linea, sizeof(linea), stdin)) continue;
        if (sscanf(linea, "%f", &v) == 1 && v >= min && v <= max) return v;
        printf("Entrada invalida. Intenta de nuevo (%.1f a %.1f).\n", min, max);
    }
}

void inicializarZonas(Zona zonas[]) {
    const char *nombres[ZONAS] = {
        "Centro Historico",
        "La Mariscal",
        "Carapungo",
        "Calderon",
        "Quitumbe"
    };

    for (int i = 0; i < ZONAS; i++) {
        strncpy(zonas[i].nombre, nombres[i], sizeof(zonas[i].nombre) - 1);
        zonas[i].nombre[sizeof(zonas[i].nombre) - 1] = '\0';
        zonas[i].climaActual.temp = 18.0f;
        zonas[i].climaActual.viento = 2.0f;
        zonas[i].climaActual.humedad = 60.0f;

        for (int c = 0; c < CONT; c++) {
            zonas[i].promedio30[c] = 0.0f;
            zonas[i].actual[c] = 0.0f;
            zonas[i].pred24h[c] = 0.0f;
            for (int d = 0; d < DIAS; d++) {
                zonas[i].hist[c][d] = 0.0f;
            }
        }
    }
}

// Genera datos simulados "realistas" para no arrancar en blanco
void generarDatosSimulados(Zona zonas[]) {
    srand((unsigned)time(NULL));
    for (int i = 0; i < ZONAS; i++) {
        for (int d = 0; d < DIAS; d++) {
            // PM2.5: 8-35
            zonas[i].hist[PM25][d] = 8.0f + (rand() % 2800) / 100.0f + (float)i;
            // NO2: 10-70
            zonas[i].hist[NO2][d]  = 10.0f + (rand() % 6000) / 100.0f + (float)(i * 0.5f);
            // SO2: 5-55
            zonas[i].hist[SO2][d]  = 5.0f + (rand() % 5000) / 100.0f + (float)(i * 0.3f);
            // CO2: 450-1400
            zonas[i].hist[CO2][d]  = 450.0f + (rand() % 9500) / 10.0f + (float)(i * 15.0f);
        }
    }
}

// ---------- Persistencia ----------
int guardarDatos(const Zona zonas[]) {
    FILE *f = fopen(ARCHIVO_DATOS, "w");
    if (!f) return 0;

    // Formato CSV:
    // zona, dia(1..30), pm25, no2, so2, co2, temp, viento, humedad
    fprintf(f, "zona,dia,pm25,no2,so2,co2,temp,viento,humedad\n");

    for (int i = 0; i < ZONAS; i++) {
        for (int d = 0; d < DIAS; d++) {
            fprintf(f, "%s,%d,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\n",
                    zonas[i].nombre, d + 1,
                    zonas[i].hist[PM25][d],
                    zonas[i].hist[NO2][d],
                    zonas[i].hist[SO2][d],
                    zonas[i].hist[CO2][d],
                    zonas[i].climaActual.temp,
                    zonas[i].climaActual.viento,
                    zonas[i].climaActual.humedad);
        }
    }
    fclose(f);
    return 1;
}

int cargarDatos(Zona zonas[]) {
    FILE *f = fopen(ARCHIVO_DATOS, "r");
    if (!f) return 0;

    char linea[256];
    // Saltar encabezado
    if (!fgets(linea, sizeof(linea), f)) { fclose(f); return 0; }

    while (fgets(linea, sizeof(linea), f)) {
        char zonaNombre[80];
        int dia;
        float pm, n, s, co, t, v, h;

        // Nota: nombre de zona sin comas
        if (sscanf(linea, "%79[^,],%d,%f,%f,%f,%f,%f,%f,%f",
                   zonaNombre, &dia, &pm, &n, &s, &co, &t, &v, &h) == 9) {

            // buscar zona
            int idx = -1;
            for (int i = 0; i < ZONAS; i++) {
                if (strcmp(zonaNombre, zonas[i].nombre) == 0) {
                    idx = i; break;
                }
            }
            if (idx != -1 && dia >= 1 && dia <= DIAS) {
                int d = dia - 1;
                zonas[idx].hist[PM25][d] = pm;
                zonas[idx].hist[NO2][d]  = n;
                zonas[idx].hist[SO2][d]  = s;
                zonas[idx].hist[CO2][d]  = co;
                zonas[idx].climaActual.temp = t;
                zonas[idx].climaActual.viento = v;
                zonas[idx].climaActual.humedad = h;
            }
        }
    }

    fclose(f);
    return 1;
}

// ---------- Cálculos ----------
float promedioArreglo(const float *arr, int n) {
    float suma = 0.0f;
    const float *p = arr;
    for (int i = 0; i < n; i++, p++) suma += *p;
    return (n > 0) ? (suma / n) : 0.0f;
}

// Promedio ponderado últimos N días: pesos 1..N (más peso al más reciente)
float promedioPonderadoUltimos(const float *arr30, int nTotal, int nUltimos) {
    if (nUltimos > nTotal) nUltimos = nTotal;
    int inicio = nTotal - nUltimos;

    float sumaPesos = 0.0f;
    float suma = 0.0f;
    int peso = 1;
    for (int i = inicio; i < nTotal; i++) {
        suma += arr30[i] * (float)peso;
        sumaPesos += (float)peso;
        peso++;
    }
    return (sumaPesos > 0) ? (suma / sumaPesos) : 0.0f;
}

// Ajuste simple por clima (solo para hacer más "realista" sin librerías avanzadas)
float ajustarPorClima(float valor, Clima c) {
    // viento alto dispersa -> baja un poco
    if (c.viento >= 4.0f) valor *= 0.92f;

    // viento muy bajo + humedad alta -> sube un poco (acumulación)
    if (c.viento < 1.5f && c.humedad >= 75.0f) valor *= 1.08f;

    // temperatura muy baja (inversiones) -> leve incremento
    if (c.temp <= 12.0f) valor *= 1.05f;

    return valor;
}

void calcularPromediosYActual(Zona zonas[]) {
    for (int i = 0; i < ZONAS; i++) {
        for (int c = 0; c < CONT; c++) {
            zonas[i].promedio30[c] = promedioArreglo(zonas[i].hist[c], DIAS);
            zonas[i].actual[c] = zonas[i].hist[c][DIAS - 1]; // último día como "actual"
        }
    }
}

void calcularPrediccion24h(Zona zonas[]) {
    for (int i = 0; i < ZONAS; i++) {
        for (int c = 0; c < CONT; c++) {
            float base = promedioPonderadoUltimos(zonas[i].hist[c], DIAS, 7);
            zonas[i].pred24h[c] = ajustarPorClima(base, zonas[i].climaActual);
        }
    }
}

// ---------- Alertas & Recomendaciones ----------
const char* nivelAlerta(float valor, float limite) {
    if (valor < 0.80f * limite) return "Normal";
    if (valor < limite)         return "Preventiva";
    if (valor < 1.20f * limite) return "Alerta";
    return "Critica";
}

void imprimirRecomendaciones(const char *nivel) {
    if (strcmp(nivel, "Normal") == 0) {
        printf("Recomendaciones: Mantener medidas de movilidad y control basico.\n");
    } else if (strcmp(nivel, "Preventiva") == 0) {
        printf("Recomendaciones: Reducir viajes innecesarios, promover transporte publico, evitar quema de residuos.\n");
        printf("               Poblacion vulnerable: considerar mascarilla y reducir exposicion prolongada.\n");
    } else if (strcmp(nivel, "Alerta") == 0) {
        printf("Recomendaciones: Control temporal de trafico (horas pico), incentivar teletrabajo, informar a la comunidad.\n");
        printf("               Suspender o reprogramar actividad fisica intensa al aire libre.\n");
    } else { // Critica
        printf("Recomendaciones: Medidas urgentes: restriccion vehicular temporal, control de fuentes fijas, alerta sanitaria.\n");
        printf("               Evitar actividades al aire libre, prioridad a ninos y adultos mayores.\n");
    }
}

// ---------- Reporte ----------
int exportarReporte(const Zona zonas[]) {
    FILE *f = fopen(ARCHIVO_REPORTE, "w");
    if (!f) return 0;

    time_t ahora = time(NULL);
    fprintf(f, "REPORTE - Sistema de Contaminacion (Quito, Ecuador)\n");
    fprintf(f, "Fecha/Hora: %s\n", ctime(&ahora));

    for (int i = 0; i < ZONAS; i++) {
        fprintf(f, "=====================================================\n");
        fprintf(f, "ZONA: %s\n", zonas[i].nombre);
        fprintf(f, "Clima Actual: Temp=%.2f C | Viento=%.2f m/s | Humedad=%.2f %%\n",
                zonas[i].climaActual.temp, zonas[i].climaActual.viento, zonas[i].climaActual.humedad);

        for (int c = 0; c < CONT; c++) {
            const char *nivAct = nivelAlerta(zonas[i].actual[c], LIMITES[c]);
            const char *nivPred = nivelAlerta(zonas[i].pred24h[c], LIMITES[c]);

            fprintf(f, "\n- %s\n", NOMBRES_CONT[c]);
            fprintf(f, "  Actual: %.2f | Limite: %.2f | Nivel: %s\n", zonas[i].actual[c], LIMITES[c], nivAct);
            fprintf(f, "  Promedio 30 dias: %.2f\n", zonas[i].promedio30[c]);
            fprintf(f, "  Prediccion 24h: %.2f | Nivel Previsto: %s\n", zonas[i].pred24h[c], nivPred);
        }
        fprintf(f, "\n");
    }

    fclose(f);
    return 1;
}

// ---------- Menú ----------
void mostrarZonas() {
    printf("\nZonas (Quito - Ecuador):\n");
    printf("1) Centro Historico\n");
    printf("2) La Mariscal\n");
    printf("3) Carapungo\n");
    printf("4) Calderon\n");
    printf("5) Quitumbe\n");
}

void ingresarClima(Zona zonas[]) {
    int z = leerEnteroSeguro("Elige zona (1-5): ", 1, 5) - 1;
    zonas[z].climaActual.temp = leerFloatSeguro("Temperatura (0 a 35 C): ", 0.0f, 35.0f);
    zonas[z].climaActual.viento = leerFloatSeguro("Viento (0 a 15 m/s): ", 0.0f, 15.0f);
    zonas[z].climaActual.humedad = leerFloatSeguro("Humedad (0 a 100 %): ", 0.0f, 100.0f);

    printf("Clima actualizado para %s.\n", zonas[z].nombre);
}

void ingresarDatoActual(Zona zonas[]) {
    int z = leerEnteroSeguro("Elige zona (1-5): ", 1, 5) - 1;
    printf("Ingresar datos ACTUALES (se guardaran como el dia 30):\n");
    for (int c = 0; c < CONT; c++) {
        char msg[80];
        snprintf(msg, sizeof(msg), "Valor actual %s: ", NOMBRES_CONT[c]);
        float v = leerFloatSeguro(msg, 0.0f, 10000.0f);
        zonas[z].hist[c][DIAS - 1] = v;
    }
    printf("Datos actuales ingresados para %s.\n", zonas[z].nombre);
}

void verResumen(Zona zonas[]) {
    calcularPromediosYActual(zonas);
    calcularPrediccion24h(zonas);

    printf("\n================ RESUMEN =================\n");
    for (int i = 0; i < ZONAS; i++) {
        printf("\nZONA: %s\n", zonas[i].nombre);
        printf("Clima: Temp=%.1fC Viento=%.1fm/s Humedad=%.1f%%\n",
               zonas[i].climaActual.temp, zonas[i].climaActual.viento, zonas[i].climaActual.humedad);

        for (int c = 0; c < CONT; c++) {
            const char *nivAct = nivelAlerta(zonas[i].actual[c], LIMITES[c]);
            const char *nivPred = nivelAlerta(zonas[i].pred24h[c], LIMITES[c]);

            printf("  %s | Actual: %.2f (%s) | Pred24h: %.2f (%s) | Prom30: %.2f | Lim: %.2f\n",
                   NOMBRES_CONT[c],
                   zonas[i].actual[c], nivAct,
                   zonas[i].pred24h[c], nivPred,
                   zonas[i].promedio30[c],
                   LIMITES[c]);

            // Si actual o pred superan preventivo, mostrar recomendaciones rápidas
            if (strcmp(nivAct, "Normal") != 0 || strcmp(nivPred, "Normal") != 0) {
                printf("    -> Recomendaciones segun nivel mas alto:\n");
                // escoger el nivel "mas grave"
                const char *nivelFinal = nivPred;
                if (strcmp(nivAct, "Critica") == 0) nivelFinal = "Critica";
                else if (strcmp(nivAct, "Alerta") == 0 && strcmp(nivPred, "Critica") != 0) nivelFinal = "Alerta";
                else if (strcmp(nivAct, "Preventiva") == 0 && strcmp(nivPred, "Normal") == 0) nivelFinal = "Preventiva";

                imprimirRecomendaciones(nivelFinal);
            }
        }
    }
}

int main() {
    Zona zonas[ZONAS];
    inicializarZonas(zonas);

    // Intentar cargar; si no existe, generar simulados y guardar
    if (cargarDatos(zonas)) {
        printf("Datos cargados desde '%s'.\n", ARCHIVO_DATOS);
    } else {
        printf("No se encontro '%s'. Se generaran datos simulados iniciales.\n", ARCHIVO_DATOS);
        generarDatosSimulados(zonas);
        if (guardarDatos(zonas)) printf("Datos iniciales guardados en '%s'.\n", ARCHIVO_DATOS);
    }

    while (1) {
        printf("\n================ MENU (ALT 1: STRUCTS) ================\n");
        printf("1) Mostrar zonas\n");
        printf("2) Ingresar/actualizar clima de una zona\n");
        printf("3) Ingresar datos actuales (dia 30) para una zona\n");
        printf("4) Calcular resumen (actual, prom30, pred24h, alertas)\n");
        printf("5) Guardar datos en archivo\n");
        printf("6) Exportar reporte a archivo\n");
        printf("7) Salir\n");

        int op = leerEnteroSeguro("Opcion: ", 1, 7);

        if (op == 1) {
            mostrarZonas();
            pausar();
        } else if (op == 2) {
            mostrarZonas();
            ingresarClima(zonas);
            pausar();
        } else if (op == 3) {
            mostrarZonas();
            ingresarDatoActual(zonas);
            pausar();
        } else if (op == 4) {
            verResumen(zonas);
            pausar();
        } else if (op == 5) {
            if (guardarDatos(zonas)) printf("Guardado OK en '%s'.\n", ARCHIVO_DATOS);
            else printf("Error al guardar.\n");
            pausar();
        } else if (op == 6) {
            calcularPromediosYActual(zonas);
            calcularPrediccion24h(zonas);
            if (exportarReporte(zonas)) printf("Reporte exportado a '%s'.\n", ARCHIVO_REPORTE);
            else printf("Error al exportar reporte.\n");
            pausar();
        } else {
            // salir
            printf("Saliendo...\n");
            break;
        }
    }

    return 0;
}
