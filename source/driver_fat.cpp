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
    if (nroSectoresPorFat != 0) {
        return CODERROR_FILESYSTEM_DESCONOCIDO; /*fat32 tiene este campo en cero y el tamaño lo guarda en el offset 0x024*/
    }
    
    /*datos que se tienen que calcular a partir de los levantados del superbloque*/
    unsigned int bytesPorCluster = bytesPorSector * sectoresPorCluster;
    unsigned int nroDeClusters = totalSectores/sectoresPorCluster;

    /*rellenar datos*/
    /*dtos del superbloque*/
    DatosFS.TipoFilesystem = tfsFAT32;
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
    DatosFS.DatosEspecificos.FAT.ClustersRootDir = 0;
    DatosFS.DatosEspecificos.FAT.PrimerClusterRootDir = primerClusterRootDir;

    return CODERROR_NINGUNO;
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
    const TDatosFSFAT &fat = DatosFS.DatosEspecificos.FAT;

    unsigned int primerSectorDatos = fat.SectoresReservados + fat.CopiasFAT * fat.SectoresPorFAT;
    unsigned int cluster = fat.PrimerClusterRootDir;
    unsigned int sector = primerSectorDatos + (cluster - 2) * fat.SectoresPorCluster;

    const unsigned char *datos = PunteroASector(sector);

    const TDirEntryFat *entrada = reinterpret_cast<const TDirEntryFat *>(datos);

    if (datos == nullptr)
        return CODERROR_SUPERBLOQUE_INVALIDO;

    unsigned int cantidadEntradas = DatosFS.BytesPorSector / 32;

    for (unsigned int i = 0; i < cantidadEntradas; i++)
    {
        const TDirEntryFAT *entrada =
            reinterpret_cast<const TDirEntryFAT *>(
                datos + i * 32
            );

        if ((unsigned char)entrada->Name[0] == 0x00)
            break;

        if ((unsigned char)entrada->Name[0] == 0xE5)
            continue;

        if (entrada->FileAttributes == FAT_LFN)
            continue;

        // Acá vamos a procesar cada entrada válida.
    }

    /* Salir */
return(CODERROR_NO_IMPLEMENTADO);
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
/* Salir */
return(CODERROR_NO_IMPLEMENTADO);
}

