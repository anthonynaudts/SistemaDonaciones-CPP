#include <iostream>
#include <sqlite3.h>
#include <string>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <mutex>
#include <thread>
#include <future>

std::mutex dbMutex;


int obtenerIdDonantePorCedula(sqlite3* db, const std::string& cedula) {
    std::ostringstream sql;
    sql << "SELECT id FROM donantes WHERE cedula = '" << cedula << "';";

    sqlite3_stmt* stmt;
    int resultado = sqlite3_prepare_v2(db, sql.str().c_str(), -1, &stmt, 0);
    if (resultado != SQLITE_OK) {
        std::cerr << "Error al obtener ID de donante: " << sqlite3_errmsg(db) << std::endl;
        return -1;
    }

    resultado = sqlite3_step(stmt);
    int idDonante = -1;
    if (resultado == SQLITE_ROW) {
        idDonante = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);
    return idDonante;
}

void registrarTransaccion(sqlite3* db, int idDonante, double monto, const std::string& numeroTarjeta, const std::string& cvv, const std::string& fechaCaducidad, int idCampania) {
    std::lock_guard<std::mutex> lock(dbMutex);

    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);

    std::tm localTime;
    localtime_s(&localTime, &time);

    std::stringstream ss;
    ss << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");
    std::string fechaTransaccion = ss.str();

    std::ostringstream sql;
    sql << "INSERT INTO transacciones (idDonante, monto, numeroTarjeta, cvv, fechaCaducidad, fechaTransaccion, idCampania) VALUES ("
        << idDonante << ", " << monto << ", '" << numeroTarjeta << "', '" << cvv << "', '" << fechaCaducidad << "', '" << fechaTransaccion << "', " << idCampania << ");";

    char* mensajeError;
    int resultado = sqlite3_exec(db, sql.str().c_str(), 0, 0, &mensajeError);
    if (resultado != SQLITE_OK) {
        std::cerr << "Error al registrar transacción: " << mensajeError << std::endl;
        sqlite3_free(mensajeError);
    }
    else {
        std::cout << "Transacción registrada exitosamente.\n";
    }
}

void registrarDonacion(sqlite3* db, int idDonante, int idCampania, double monto) {
    if (monto <= 0) {
        std::cout << "El monto de la donación debe ser mayor que cero." << std::endl;
        return;
    }

    std::ostringstream sqlInsertarDonacion;
    sqlInsertarDonacion << "INSERT INTO donaciones (idDonante, idCampania, monto, fecha) VALUES ("
        << idDonante << ", " << idCampania << ", " << monto << ", datetime('now'));";

    char* mensajeError;
    int resultado = sqlite3_exec(db, sqlInsertarDonacion.str().c_str(), 0, 0, &mensajeError);
    if (resultado != SQLITE_OK) {
        std::cerr << "Error al registrar la donación: " << mensajeError << std::endl;
        sqlite3_free(mensajeError);
        return;
    }

    std::cout << "Donación registrada exitosamente." << std::endl;
}

bool verificarDonantePorCedula(sqlite3* db, const std::string& cedula) {
    std::ostringstream sql;
    sql << "SELECT id FROM donantes WHERE cedula = '" << cedula << "';";

    sqlite3_stmt* stmt;
    int resultado = sqlite3_prepare_v2(db, sql.str().c_str(), -1, &stmt, 0);
    if (resultado != SQLITE_OK) {
        std::cerr << "Error al verificar donante: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    resultado = sqlite3_step(stmt);
    bool existe = (resultado == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return existe;
}

void registrarDonante(sqlite3* db) {
    std::string cedula;
    std::cout << "Ingrese la cédula del donante: ";
    std::getline(std::cin, cedula);

    if (verificarDonantePorCedula(db, cedula)) {
        std::cout << "El donante con cédula " << cedula << " ya está registrado.\n";
        return;
    }

    std::string nombre, correo, telefono, direccion;

    std::cout << "Nombre del donante: ";
    std::getline(std::cin, nombre);
    std::cout << "Correo del donante: ";
    std::getline(std::cin, correo);
    std::cout << "Teléfono del donante: ";
    std::getline(std::cin, telefono);
    std::cout << "Dirección del donante: ";
    std::getline(std::cin, direccion);

    std::ostringstream sql;
    sql << "INSERT INTO donantes (nombre, correo, telefono, direccion, cedula) VALUES ('"
        << nombre << "', '" << correo << "', '" << telefono << "', '" << direccion << "', '" << cedula << "');";

    char* mensajeError;
    int resultado = sqlite3_exec(db, sql.str().c_str(), 0, 0, &mensajeError);
    if (resultado != SQLITE_OK) {
        std::cerr << "Error al registrar donante: " << mensajeError << std::endl;
        sqlite3_free(mensajeError);
    }
    else {
        std::cout << "Donante registrado exitosamente.\n";
    }
}

void obtenerCampaniasDisponibles(sqlite3* db) {
    std::ostringstream sql;
    sql << "SELECT id, nombre, descripcion, objetivo, recaudado, fechaFin FROM campanias WHERE fechaFin > datetime('now');";

    sqlite3_stmt* stmt;
    int resultado = sqlite3_prepare_v2(db, sql.str().c_str(), -1, &stmt, 0);
    if (resultado != SQLITE_OK) {
        std::cerr << "Error al obtener campañas: " << sqlite3_errmsg(db) << std::endl;
        return;
    }

    std::cout << "\nCampañas disponibles:" << std::endl;

    while ((resultado = sqlite3_step(stmt)) == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt, 0);
        const char* nombre = (const char*)sqlite3_column_text(stmt, 1);
        const char* descripcion = (const char*)sqlite3_column_text(stmt, 2);
        double objetivo = sqlite3_column_double(stmt, 3);
        double recaudado = sqlite3_column_double(stmt, 4);
        const char* fechaFin = (const char*)sqlite3_column_text(stmt, 5);

        std::tm fechaFinTm = {};
        std::istringstream fechaStream(fechaFin);
        fechaStream >> std::get_time(&fechaFinTm, "%Y-%m-%d");
        std::time_t fechaFinT = std::mktime(&fechaFinTm);
        std::time_t ahora = std::time(nullptr);
        int diasRestantes = static_cast<int>((fechaFinT - ahora) / (60 * 60 * 24));

        std::cout << "ID: " << id << ", Nombre: " << nombre << ", Descripción: " << descripcion
            << ", Objetivo: " << objetivo << ", Recaudado: " << recaudado
            << ", Días Restantes: " << diasRestantes << std::endl;
    }
    sqlite3_finalize(stmt);
}


void actualizarMontoRecaudado(sqlite3* db, int idCampania, double monto) {
    std::lock_guard<std::mutex> lock(dbMutex);

    std::ostringstream sqlActualizarCampania;
    sqlActualizarCampania << "UPDATE campanias SET recaudado = recaudado + " << monto
        << " WHERE id = " << idCampania << ";";

    char* mensajeError;
    int resultado = sqlite3_exec(db, sqlActualizarCampania.str().c_str(), 0, 0, &mensajeError);
    if (resultado != SQLITE_OK) {
        std::cerr << "Error al actualizar la campaña: " << mensajeError << std::endl;
        sqlite3_free(mensajeError);
    }
    else {
        std::cout << "Monto recaudado actualizado." << std::endl;
    }
}

bool validarTarjeta(const std::string& numeroTarjeta, const std::string& fechaCaducidad, const std::string& cvv) {
    
    return true;
}

void realizarDonacion(sqlite3* db, int idDonante, int idCampania, double monto, const std::string& numeroTarjeta, const std::string& cvv, const std::string& fechaCaducidad) {
    if (!validarTarjeta(numeroTarjeta, fechaCaducidad, cvv)) {
        std::cout << "La tarjeta no es válida.\n";
        return;
    }

    std::thread t1(registrarDonacion, db, idDonante, idCampania, monto);
    std::thread t2(registrarTransaccion, db, idDonante, monto, numeroTarjeta, cvv, fechaCaducidad, idCampania);
    std::thread t3(actualizarMontoRecaudado, db, idCampania, monto);

    t1.join();
    t2.join();
    t3.join();
}

void crearCampania(sqlite3* db) {
    std::string nombre, descripcion, fechaInicio, fechaFin;
    double objetivo;

    std::cout << "Nombre de la campaña: ";
    std::getline(std::cin, nombre);
    std::cout << "Descripción de la campaña: ";
    std::getline(std::cin, descripcion);
    std::cout << "Objetivo de la campaña: ";
    std::cin >> objetivo;
    std::cin.ignore();
    std::cout << "Fecha de inicio (YYYY-MM-DD): ";
    std::getline(std::cin, fechaInicio);
    std::cout << "Fecha de fin (YYYY-MM-DD): ";
    std::getline(std::cin, fechaFin);

    std::ostringstream sql;
    sql << "INSERT INTO campanias (nombre, descripcion, objetivo, recaudado, fechaInicio, fechaFin) VALUES ('"
        << nombre << "', '" << descripcion << "', " << objetivo << ", 0, '" << fechaInicio << "', '" << fechaFin << "');";

    char* mensajeError;
    int resultado = sqlite3_exec(db, sql.str().c_str(), 0, 0, &mensajeError);
    if (resultado != SQLITE_OK) {
        std::cerr << "Error al crear campaña: " << mensajeError << std::endl;
        sqlite3_free(mensajeError);
    }
    else {
        std::cout << "Campaña creada exitosamente.\n";
    }
}


int main() {
    sqlite3* db;
    int resultado = sqlite3_open("sistemaDonaciones.db", &db);
    if (resultado != SQLITE_OK) {
        std::cerr << "Error al abrir la base de datos: " << sqlite3_errmsg(db) << std::endl;
        return 1;
    }

    std::setlocale(LC_ALL, "en_US.UTF-8");

    int opcion;
    do {
        std::cout << "\nMenú:\n"
            << "1. Registrar Donante\n"
            << "2. Crear Campaña\n"
            << "3. Ver Campañas Disponibles\n"
            << "4. Realizar Donación\n"
            << "5. Salir\n"
            << "Seleccione una opción: ";
        std::cin >> opcion;
        std::cin.ignore();

        switch (opcion) {
        case 1:
            registrarDonante(db);
            break;
        case 2:
            crearCampania(db);
            break;
        case 3:
            obtenerCampaniasDisponibles(db);
            break;
        case 4:
        {
            std::string cedula;
            std::cout << "Ingrese la cédula del donante: ";
            std::getline(std::cin, cedula);

            auto futureIdDonante = std::async(std::launch::async, obtenerIdDonantePorCedula, db, cedula);
            auto futureCampanias = std::async(std::launch::async, obtenerCampaniasDisponibles, db);

            int idDonante = futureIdDonante.get();
            futureCampanias.get(); 

            if (idDonante == -1) {
                std::cout << "El donante no está registrado.\n";
                break;
            }


            int idCampania;
            std::cout << "Ingrese el ID de la campaña a la que desea donar: ";
            std::cin >> idCampania;

            double monto;
            std::cout << "Ingrese el monto de la donación: ";
            std::cin >> monto;
            std::cin.ignore();

            std::string numeroTarjeta, cvv, fechaCaducidad;
            std::cout << "Ingrese el número de tarjeta: ";
            std::getline(std::cin, numeroTarjeta);
            std::cout << "Ingrese el CVV: ";
            std::getline(std::cin, cvv);
            std::cout << "Ingrese la fecha de caducidad (MM/AAAA): ";
            std::getline(std::cin, fechaCaducidad);

            realizarDonacion(db, idDonante, idCampania, monto, numeroTarjeta, cvv, fechaCaducidad);
            break;
        }
        case 5:
            std::cout << "Saliendo del sistema.\n";
            break;
        default:
            std::cout << "Opción no válida. Intente nuevamente.\n";
            break;
        }
    } while (opcion != 5);

    sqlite3_close(db);
    return 0;
}
