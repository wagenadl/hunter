/** 
 * @file exceptions.h
 * @brief Exception definitions
 *
 * @author Santiago Navonne
 *
 * This contains the definitions for all the exceptions thrown 
 * (and hopefully caught) in the program.
 */


enum exception_t {
    EXCEPTION_PG,
    EXCEPTION_INTEL,
    EXCEPTION_PG_CONFIG,
    EXCEPTION_PG_INVALID_CAM,
};