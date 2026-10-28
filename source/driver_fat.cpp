#include "all_heads.h"


/********************************
 *				*
 *	 Clase TDriverFAT	*
 *				*
 ********************************/
/****************************************************************************************************************************************
 *																	*
 *							TDriverFAT :: TDriverFAT							*
 *																	*
 * OBJETIVO: Inicializar la clase recién creada.											*
 *																	*
 * ENTRADA: DiskData: Puntero a un bloque de memoria con la imágen del disco a analizar.						*
 *	    LongitudDiskData: Tamaño, en bytes, de la imágen a analizar.								*
 *																	*
 * SALIDA: Nada.															*
 *																	*
 ****************************************************************************************************************************************/
TDriverFAT::TDriverFAT(const unsigned char *DiskData, unsigned LongitudDiskData) : TDriverBase(DiskData, LongitudDiskData)
{
}


/****************************************************************************************************************************************
 *																	*
 *							TDriverFAT :: ~TDriverFAT							*
 *																	*
 * OBJETIVO: Liberar recursos alocados.													*
 *																	*
 * ENTRADA: Nada.															*
 *																	*
 * SALIDA: Nada.															*
 *																	*
 ****************************************************************************************************************************************/
TDriverFAT::~TDriverFAT()
{
}


/****************************************************************************************************************************************
 *																	*
 *						   TDriverFAT :: LevantarDatosSuperbloque						*
 *																	*
 * OBJETIVO: Esta función analiza el superbloque y completa la estructura DatosFS con los datos levantados.				*
 *																	*
 * ENTRADA: Nada.															*
 *																	*
 * SALIDA: En el nombre de la función CODERROR_NINGUNO si no hubo errores. Sino uno de los siguientes valores:				*
 *		CODERROR_SUPERBLOQUE_INVALIDO   : El superbloque está dañado o no corresponde a un disco con ningún formato.		*
 *		CODERROR_FILESYSTEM_DESCONOCIDO : El superbloque es válido, pero no corresponde a un FyleSystem soportado por esta	*
 *						  clase.										*
 *																	*
 ****************************************************************************************************************************************/
int TDriverFAT::LevantarDatosSuperbloque()
{
/* Salir */
    const unsigned char *p = PunteroASector(0);
        if (p == NULL)
            return CODERROR_SUPERBLOQUE_INVALIDO;

    /*leer superbloque*/
    unsigned short bytesPorSector = *(const unsigned short *)(p + 11);
    unsigned short sectoresPorCluster = p[13];
    unsigned short sectoresReservados = *(const unsigned short *)(p + 14);
    unsigned short nroDeFats = p[16];
    unsigned short nroRootDirEntries = *(const unsigned short *)(p + 17);
    unsigned int totalSectores = *(const unsigned short *)(p + 19);
    if (totalSectores == 0) { /*si este valor es cero significa que tiene mas de 65535 sectores y el valor real esta en 0x20*/
        totalSectores = *(const unsigned int *)(p + 32);
    }
    unsigned short nroSectoresPorFat = *(const unsigned short *)(p + 22);
    unsigned int nroSectoresOcultos = *(const unsigned int *)(p + 28);
    unsigned int sectoresPorFat = *(const unsigned int *)(p + 36);
    unsigned int primerClusterRootDir = *(const unsigned int *)(p + 44);

    /*chequear si es fat y si es fat32*/
    if (p[510] != 0x55 || p[511] != 0xAA) return CODERROR_SUPERBLOQUE_INVALIDO; /*la firma al final del superbloque*/
    if (bytesPorSector != 512 && bytesPorSector != 1024 && bytesPorSector != 2048 && bytesPorSector != 4096) return CODERROR_SUPERBLOQUE_INVALIDO; /*únicos tamaños aceptados*/
    if (sectoresPorCluster == 0 || (sectoresPorCluster & (sectoresPorCluster - 1)) != 0) return CODERROR_SUPERBLOQUE_INVALIDO; /*sectores por cluster potencia de 2 y mayores que cero*/
    /*un numero que es potencia de 2 solo tiene un bit en 1, entonces [ spc & (spc-1) = 0 ]*/
    if (sectoresReservados == 0) return CODERROR_SUPERBLOQUE_INVALIDO; /*fat siempre tiene al menos 1 reservado (el boot sector)*/
    if (nroDeFats == 0) return CODERROR_SUPERBLOQUE_INVALIDO; /*si no tiene fats no es fat*/
    /*FAT12/16 guardan los sectores por FAT en el offset 22; FAT32 lo deja en 0 y lo guarda en el 36*/
    if (nroSectoresPorFat != 0) {
        sectoresPorFat = nroSectoresPorFat;
        primerClusterRootDir = 0;
    }

    /*cuántos sectores ocupa la zona fija del root dir (en FAT32 da 0)*/
    unsigned int sectoresRootDir = (nroRootDirEntries * 32 + bytesPorSector - 1) / bytesPorSector;

    /*el tipo de FAT se decide por la cantidad de clusters de datos*/
    unsigned int sectoresDatos = totalSectores - (sectoresReservados + nroDeFats * sectoresPorFat + sectoresRootDir);
    unsigned int clustersDatos = sectoresDatos / sectoresPorCluster;
    TipoFilsystem tipo;
    if (clustersDatos < 4085) tipo = tfsFAT12;
    else if (clustersDatos < 65525) tipo = tfsFAT16;
    else tipo = tfsFAT32;

    /*datos que se tienen que calcular a partir de los levantados del superbloque*/
    unsigned int bytesPorCluster = bytesPorSector * sectoresPorCluster;
    unsigned int nroDeClusters = totalSectores/sectoresPorCluster;

    /*rellenar datos*/
    /*dtos del superbloque*/
    DatosFS.TipoFilesystem = tipo;
    DatosFS.BytesPorSector = bytesPorSector;
    DatosFS.NumeroDeClusters = nroDeClusters;
    DatosFS.BytesPorCluster = bytesPorCluster;

    /*datos específicos*/
    DatosFS.DatosEspecificos.FAT.SectoresPorCluster = sectoresPorCluster;
    DatosFS.DatosEspecificos.FAT.SectoresReservados = sectoresReservados;
    DatosFS.DatosEspecificos.FAT.CopiasFAT = nroDeFats;
    DatosFS.DatosEspecificos.FAT.EntradasRootDir = nroRootDirEntries;
    DatosFS.DatosEspecificos.FAT.SectoresPorFAT = sectoresPorFat;
    DatosFS.DatosEspecificos.FAT.TotalSectores = totalSectores;
    DatosFS.DatosEspecificos.FAT.SectoresOcultos = nroSectoresOcultos;
    DatosFS.DatosEspecificos.FAT.ClustersRootDir = (nroRootDirEntries * 32 + bytesPorCluster - 1) / bytesPorCluster;
    DatosFS.DatosEspecificos.FAT.PrimerClusterRootDir = primerClusterRootDir;

    return CODERROR_NINGUNO;
}

/* Convierte fecha y hora de FAT a time_t. Si la fecha es 0, no hay fecha. */
static time_t FechaFATaTime(__u16 fecha, __u16 hora)
{
    if (fecha == 0) return 0;
    struct tm t = {};
    t.tm_year = ((fecha >> 9) & 0x7F) + 80;   /* FAT cuenta desde 1980, tm desde 1900 */
    t.tm_mon  = ((fecha >> 5) & 0x0F) - 1;    /* tm cuenta meses desde 0 */
    t.tm_mday = fecha & 0x1F;
    t.tm_hour = (hora >> 11) & 0x1F;
    t.tm_min  = (hora >> 5) & 0x3F;
    t.tm_sec  = (hora & 0x1F) * 2;            /* FAT guarda segundos / 2 */
    t.tm_isdst = -1;
    return mktime(&t);
}

/* Convierte una entrada de directorio FAT a la estructura genérica del programa */
static TEntradaDirectorio ConvertirEntrada(const TDirEntryFAT *e)
{
    TEntradaDirectorio r = {};

    /* Nombre: 8 caracteres + 3 de extensión, rellenos con espacios */
    std::string nombre(e->Name, 8);
    std::string ext(e->Ext, 3);
    while (!nombre.empty() && nombre.back() == ' ') nombre.pop_back();
    while (!ext.empty() && ext.back() == ' ') ext.pop_back();
    r.Nombre = ext.empty() ? nombre : nombre + "." + ext;

    /* Flags: en FAT los bits coinciden con los fed... del programa */
    r.Flags = e->FileAttributes & (FAT_READ_ONLY | FAT_HIDDEN | FAT_SYSTEM | FAT_VOLUME_ID | FAT_DIRECTORY | FAT_ARCHIVE);

    r.Bytes = (__u32)e->FileSize;
    r.DatosEspecificos.FAT.PrimerCluster = ((__u32)e->StartClusterH << 16) | e->StartClusterL;

    r.FechaCreacion = FechaFATaTime(e->CreationDate, e->CreationTime);
    r.FechaUltimoAcceso = FechaFATaTime(e->LastAccessDate, 0);
    r.FechaUltimaModificacion = FechaFATaTime(e->ModificationDate, e->ModificationTime);
    return r;
}


/****************************************************************************************************************************************
 *																	*
 *						  TDriverFAT :: ListarDirectorio							*
 *																	*
 * OBJETIVO: Esta función enumera las entradas en un directorio y retorna un arreglo de elementos, uno por cada entrada.		*
 *																	*
 * ENTRADA: Path: Path al directorio enumerar (cadena de nombres de directorio separados por '/').					*
 *																	*
 * SALIDA: En el nombre de la función CODERROR_NINGUNO si no hubo errores, caso contrario el código de error.				*
 *	   Entradas: Arreglo con cada una de las entradas.										*
 *																	*
 ****************************************************************************************************************************************/
int TDriverFAT::ListarDirectorio(const char *Path, std::vector<TEntradaDirectorio> &Entradas)
{
    TEntradaDirectorio dir;
    int cod = BuscarEntrada(Path, dir);
    if (cod != CODERROR_NINGUNO)
        return CODERROR_DIRECTORIO_INEXISTENTE;
    if (!(dir.Flags & fedDIRECTORIO))
        return CODERROR_DIRECTORIO_INEXISTENTE;   /* existe, pero es un archivo */

    return LeerDirectorio(dir.DatosEspecificos.FAT.PrimerCluster, Entradas);
}

/* Recorre el path y devuelve la entrada final. Para "/" devuelve un directorio con cluster 0 (el root). */
int TDriverFAT::BuscarEntrada(const char *Path, TEntradaDirectorio &Encontrada)
{
    if (Path == NULL || Path[0] != '/')
        return CODERROR_RUTA_NO_ABSOLUTA;

    /* Arrancamos en el root */
    Encontrada = TEntradaDirectorio();
    Encontrada.Flags = fedDIRECTORIO;
    Encontrada.DatosEspecificos.FAT.PrimerCluster = 0;

    std::string path(Path);
    size_t pos = 1;
    while (pos <= path.size()) {
        /* Sacar la próxima parte del path, entre dos '/' */
        size_t barra = path.find('/', pos);
        if (barra == std::string::npos) barra = path.size();
        std::string parte = path.substr(pos, barra - pos);
        pos = barra + 1;
        if (parte.empty()) continue;            /* "//" o "/" al final */

        /* Solo se puede entrar a un directorio */
        if (!(Encontrada.Flags & fedDIRECTORIO))
            return CODERROR_ARCHIVO_INEXISTENTE;

        /* Leer el directorio actual y buscar la parte */
        std::vector<TEntradaDirectorio> entradas;
        int cod = LeerDirectorio(Encontrada.DatosEspecificos.FAT.PrimerCluster, entradas);
        if (cod != CODERROR_NINGUNO) return cod;

        bool ok = false;
        for (const auto &e : entradas) {
            if (e.Flags & fedETIQUETA_VOLUMEN) continue;   /* la etiqueta no es un archivo */
            if (strcasecmp(e.Nombre.c_str(), parte.c_str()) == 0) {
                Encontrada = e;
                ok = true;
                break;
            }
        }
        if (!ok) return CODERROR_ARCHIVO_INEXISTENTE;
    }
    return CODERROR_NINGUNO;
}


/* Número de cluster → puntero a sus datos en la imagen */
const unsigned char *TDriverFAT::PunteroACluster(unsigned Cluster)
{
    const TDatosFSFAT &fat = DatosFS.DatosEspecificos.FAT;
    unsigned sectoresRootDir = (fat.EntradasRootDir * 32 + DatosFS.BytesPorSector - 1) / DatosFS.BytesPorSector;
    unsigned primerSectorDatos = fat.SectoresReservados + fat.CopiasFAT * fat.SectoresPorFAT + sectoresRootDir;
    return PunteroASector(primerSectorDatos + (__u64)(Cluster - 2) * fat.SectoresPorCluster);
}


/* Devuelve el cluster que sigue a Cluster según la tabla FAT. Devuelve 0 si es el último. */
unsigned TDriverFAT::SiguienteCluster(unsigned Cluster)
{
    const TDatosFSFAT &fat = DatosFS.DatosEspecificos.FAT;
    const unsigned char *tabla = PunteroASector(fat.SectoresReservados);
    if (tabla == NULL) return 0;

    unsigned sig;
    switch (DatosFS.TipoFilesystem) {
        case tfsFAT12: {
            unsigned offset = Cluster + Cluster / 2;        /* 1,5 bytes por entrada */
            unsigned par = tabla[offset] | (tabla[offset + 1] << 8);
            sig = (Cluster & 1) ? (par >> 4) : (par & 0x0FFF);
            if (sig >= 0xFF8) return 0;
            break;
        }
        case tfsFAT16:
            sig = ((const __u16 *)tabla)[Cluster];
            if (sig >= 0xFFF8) return 0;
            break;
        default:
            sig = ((const __u32 *)tabla)[Cluster] & 0x0FFFFFFF;   /* FAT32 usa solo 28 bits */
            if (sig >= 0x0FFFFFF8) return 0;
            break;
    }
    if (sig < 2) return 0;
    return sig;
}


/* Lee todas las entradas de un directorio. PrimerCluster == 0 significa el root. */
int TDriverFAT::LeerDirectorio(unsigned PrimerCluster, std::vector<TEntradaDirectorio> &Entradas)
{
    const TDatosFSFAT &fat = DatosFS.DatosEspecificos.FAT;

    /* Procesa un bloque de bytes con entradas de 32 bytes. Devuelve false si encontró el fin (0x00). */
    auto ProcesarBloque = [&](const unsigned char *datos, unsigned bytes) -> bool {
        for (unsigned i = 0; i < bytes / 32; i++) {
            const TDirEntryFAT *entrada = reinterpret_cast<const TDirEntryFAT *>(datos + i * 32);
            if ((unsigned char)entrada->Name[0] == 0x00) return false;
            if ((unsigned char)entrada->Name[0] == 0xE5) continue;
            if ((entrada->FileAttributes & FAT_LFN) == FAT_LFN) continue;
            Entradas.push_back(ConvertirEntrada(entrada));
        }
        return true;
    };

    /* Root de FAT12/16: zona fija, no es un cluster */
    if (PrimerCluster == 0 && DatosFS.TipoFilesystem != tfsFAT32) {
        const unsigned char *datos = PunteroASector(fat.SectoresReservados + fat.CopiasFAT * fat.SectoresPorFAT);
        if (datos == NULL) return CODERROR_FILESYSTEM_CORRUPTO;
        ProcesarBloque(datos, fat.EntradasRootDir * 32);
        return CODERROR_NINGUNO;
    }

    /* Root de FAT32: es un cluster más */
    if (PrimerCluster == 0)
        PrimerCluster = fat.PrimerClusterRootDir;

    /* Seguir la cadena de clusters */
    unsigned cluster = PrimerCluster;
    while (cluster != 0) {
        const unsigned char *datos = PunteroACluster(cluster);
        if (datos == NULL) return CODERROR_FILESYSTEM_CORRUPTO;
        if (!ProcesarBloque(datos, DatosFS.BytesPorCluster)) break;
        cluster = SiguienteCluster(cluster);
    }
    return CODERROR_NINGUNO;
}


/****************************************************************************************************************************************
 *																	*
 *						     TDriverFAT :: LeerArchivo								*
 *																	*
 * OBJETIVO: Esta función levanta de la imágen un archivo dada su ruta.									*
 *																	*
 * ENTRADA: Path: Ruta al archivo a levantar.												*
 *																	*
 * SALIDA: En el nombre de la función CODERROR_NINGUNO si no hubo errores, caso contrario el código de error.				*
 *	   Data: Buffer alocado con malloc() con los datos del archivo.									*
 *	   DataLen: Tamaño en bytes del buffer devuelto.										*
 *																	*
 * OBSERVACIONES: Los valores Data y DataLen sólo devuelven valores válidos si se retorna CODERROR_NINGUNO.				*
 *																	*
 ****************************************************************************************************************************************/
int TDriverFAT::LeerArchivo(const char *Path, unsigned char *&Data, unsigned &DataLen)
{
    /* 1. Buscar el archivo */
    TEntradaDirectorio arch;
    int cod = BuscarEntrada(Path, arch);
    if (cod != CODERROR_NINGUNO || (arch.Flags & fedDIRECTORIO))
        return CODERROR_ARCHIVO_INEXISTENTE;

    /* 2. Reservar memoria para todo el archivo */
    unsigned tam = (unsigned)arch.Bytes;
    Data = (unsigned char *)malloc(tam > 0 ? tam : 1);
    if (Data == NULL) return CODERROR_FALTA_MEMORIA;

    /* 3. Copiar cluster por cluster siguiendo la cadena */
    unsigned copiados = 0;
    unsigned cluster = arch.DatosEspecificos.FAT.PrimerCluster;
    while (copiados < tam && cluster != 0) {
        const unsigned char *datos = PunteroACluster(cluster);
        if (datos == NULL) { free(Data); Data = NULL; return CODERROR_FILESYSTEM_CORRUPTO; }

        unsigned n = tam - copiados;                      /* lo que falta */
        if (n > (unsigned)DatosFS.BytesPorCluster)
            n = DatosFS.BytesPorCluster;                  /* como mucho, un cluster */
        memcpy(Data + copiados, datos, n);
        copiados += n;

        cluster = SiguienteCluster(cluster);
    }

    /* Si la cadena terminó antes de tiempo, el FS está roto */
    if (copiados < tam) { free(Data); Data = NULL; return CODERROR_FILESYSTEM_CORRUPTO; }

    DataLen = tam;
    return CODERROR_NINGUNO;
}

