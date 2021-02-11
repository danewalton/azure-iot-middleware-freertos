/* Copyright (c) Microsoft Corporation. All rights reserved. */
/* SPDX-License-Identifier: MIT */

/**
 * @file azure_iot.c
 * @brief -------.
 *
 */

#include "azure_iot.h"

static const char _azure_iot_base64_array[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void azure_iot_log_listener( az_log_classification classification, az_span message )
{
    ( void ) classification;

    AZLogInfo( ( "%.*s", az_span_size( message ), az_span_ptr( message ) ) );
}

static AzureIoTError_t azure_iot_base64_decode( char * base64name,
                                                uint32_t length,
                                                uint8_t * name,
                                                uint32_t name_size,
                                                uint32_t * bytes_copied )
{
    uint32_t i, j;
    uint32_t value1, value2;
    uint32_t step;
    uint32_t sourceLength = length;

    /* Adjust the length to represent the ASCII name.  */
    length = ( ( length * 6 ) / 8 );

    if( base64name[ sourceLength - 1 ] == '=' )
    {
        if( base64name[ sourceLength - 2 ] == '=' )
        {
            length--;
        }

        length--;
    }

    if( name_size < length )
    {
        return AZURE_IOT_STATUS_OOM;
    }

    /* Setup index into the ASCII name.  */
    j = 0;

    /* Compute the ASCII name.  */
    step = 0;
    i = 0;

    while( ( j < length ) && ( base64name[ i ] ) && ( base64name[ i ] != '=' ) )
    {
        /* Derive values of the Base64 name.  */
        if( ( base64name[ i ] >= 'A' ) && ( base64name[ i ] <= 'Z' ) )
        {
            value1 = ( uint32_t ) ( base64name[ i ] - 'A' );
        }
        else if( ( base64name[ i ] >= 'a' ) && ( base64name[ i ] <= 'z' ) )
        {
            value1 = ( uint32_t ) ( base64name[ i ] - 'a' ) + 26;
        }
        else if( ( base64name[ i ] >= '0' ) && ( base64name[ i ] <= '9' ) )
        {
            value1 = ( uint32_t ) ( base64name[ i ] - '0' ) + 52;
        }
        else if( base64name[ i ] == '+' )
        {
            value1 = 62;
        }
        else if( base64name[ i ] == '/' )
        {
            value1 = 63;
        }
        else
        {
            value1 = 0;
        }

        /* Derive value for the next character.  */
        if( ( base64name[ i + 1 ] >= 'A' ) && ( base64name[ i + 1 ] <= 'Z' ) )
        {
            value2 = ( uint32_t ) ( base64name[ i + 1 ] - 'A' );
        }
        else if( ( base64name[ i + 1 ] >= 'a' ) && ( base64name[ i + 1 ] <= 'z' ) )
        {
            value2 = ( uint32_t ) ( base64name[ i + 1 ] - 'a' ) + 26;
        }
        else if( ( base64name[ i + 1 ] >= '0' ) && ( base64name[ i + 1 ] <= '9' ) )
        {
            value2 = ( uint32_t ) ( base64name[ i + 1 ] - '0' ) + 52;
        }
        else if( base64name[ i + 1 ] == '+' )
        {
            value2 = 62;
        }
        else if( base64name[ i + 1 ] == '/' )
        {
            value2 = 63;
        }
        else
        {
            value2 = 0;
        }

        /* Determine which step we are in.  */
        if( step == 0 )
        {
            /* Use first value and first 2 bits of second value.  */
            name[ j++ ] = ( uint8_t ) ( ( ( value1 & 0x3f ) << 2 ) | ( ( value2 >> 4 ) & 3 ) );
            i++;
            step++;
        }
        else if( step == 1 )
        {
            /* Use last 4 bits of first value and first 4 bits of next value.  */
            name[ j++ ] = ( uint8_t ) ( ( ( value1 & 0xF ) << 4 ) | ( value2 >> 2 ) );
            i++;
            step++;
        }
        else if( step == 2 )
        {
            /* Use first 2 bits and following 6 bits of next value.  */
            name[ j++ ] = ( uint8_t ) ( ( ( value1 & 3 ) << 6 ) | ( value2 & 0x3f ) );
            i++;
            i++;
            step = 0;
        }
    }

    /* Put a NULL character in.  */
    name[ j ] = 0;
    *bytes_copied = j;

    return AZURE_IOT_SUCCESS;
}

static AzureIoTError_t azure_iot_base64_encode( uint8_t * name,
                                                uint32_t length,
                                                char * base64name,
                                                uint32_t base64name_size )
{
    uint32_t pad;
    uint32_t i, j;
    uint32_t step;


    /* Adjust the length to represent the base64 name.  */
    length = ( ( length * 8 ) / 6 );

    /* Default padding to none.  */
    pad = 0;

    /* Determine if an extra conversion is needed.  */
    if( ( length * 6 ) % 24 )
    {
        /* Some padding is needed.  */

        /* Calculate the number of pad characters.  */
        pad = ( length * 6 ) % 24;
        pad = ( 24 - pad ) / 6;
        pad = pad - 1;

        /* Adjust the length to pickup the character fraction.  */
        length++;
    }

    if( base64name_size <= length )
    {
        return AZURE_IOT_STATUS_OOM;
    }

    /* Setup index into the base64name.  */
    j = 0;

    /* Compute the base64name.  */
    step = 0;
    i = 0;

    while( j < length )
    {
        /* Determine which step we are in.  */
        if( step == 0 )
        {
            /* Use first 6 bits of name character for index.  */
            base64name[ j++ ] = ( char ) _azure_iot_base64_array[ ( ( uint32_t ) name[ i ] ) >> 2 ];
            step++;
        }
        else if( step == 1 )
        {
            /* Use last 2 bits of name character and first 4 bits of next name character for index.  */
            base64name[ j++ ] = ( char ) _azure_iot_base64_array[ ( ( ( ( uint32_t ) name[ i ] ) & 0x3 ) << 4 ) | ( ( ( uint32_t ) name[ i + 1 ] ) >> 4 ) ];
            i++;
            step++;
        }
        else if( step == 2 )
        {
            /* Use last 4 bits of name character and first 2 bits of next name character for index.  */
            base64name[ j++ ] = ( char ) _azure_iot_base64_array[ ( ( ( ( uint32_t ) name[ i ] ) & 0xF ) << 2 ) | ( ( ( uint32_t ) name[ i + 1 ] ) >> 6 ) ];
            i++;
            step++;
        }
        else /* Step 3 */
        {
            /* Use last 6 bits of name character for index.  */
            base64name[ j++ ] = ( char ) _azure_iot_base64_array[ ( ( ( uint32_t ) name[ i ] ) & 0x3F ) ];
            i++;
            step = 0;
        }
    }

    /* Determine if the index needs to be advanced.  */
    if( step != 3 )
    {
        i++;
    }

    /* Now add the PAD characters.  */
    while( ( pad-- ) && ( j < base64name_size ) )
    {
        /* Pad base64name with '=' characters.  */
        base64name[ j++ ] = '=';
    }

    /* Put a NULL character in.  */
    base64name[ j ] = 0;

    return AZURE_IOT_SUCCESS;
}

AzureIoTError_t AzureIoTLogInit()
{
#if ( LIBRARY_LOG_LEVEL == LOG_INFO )
    az_log_set_message_callback( azure_iot_log_listener );
#endif

    return AZURE_IOT_SUCCESS;
}

AzureIoTError_t AzureIoTMessagePropertiesInit( AzureIoTMessageProperties_t * messageProperties,
                                               uint8_t * buffer,
                                               uint32_t writtenLength,
                                               uint32_t bufferLength )
{
    az_span propertyBufferSpan = az_span_create( buffer, ( int32_t ) bufferLength );
    az_result result;

    if ( messageProperties == NULL )
    {
        AZLogError( ( "AzureIoTMessagePropertiesInit failed: Invalid argument \r\n" ) );
        return AZURE_IOT_INVALID_ARGUMENT;
    }

    result = az_iot_message_properties_init( &messageProperties->_internal.properties,
                                             propertyBufferSpan, ( int32_t ) writtenLength );
    if( az_result_failed( result ) )
    {
        return AZURE_IOT_FAILED;
    }

    return AZURE_IOT_SUCCESS;
}

AzureIoTError_t AzureIoTMessagePropertiesAppend( AzureIoTMessageProperties_t * messageProperties,
                                                 uint8_t * pName,
                                                 uint32_t nameLength,
                                                 uint8_t * pValue,
                                                 uint32_t valueLength )
{
    az_span nameSpan = az_span_create( pName, ( int32_t ) nameLength );
    az_span valueSpan = az_span_create( pValue, ( int32_t ) valueLength );
    az_result result;

    if ( messageProperties == NULL )
    {
        AZLogError( ( "AzureIoTMessagePropertiesAppend failed: Invalid argument \r\n" ) );
        return AZURE_IOT_INVALID_ARGUMENT;
    }

    result = az_iot_message_properties_append( &messageProperties->_internal.properties,
                                               nameSpan, valueSpan );
    if( az_result_failed( result ) )
    {
        return AZURE_IOT_FAILED;
    }

    return AZURE_IOT_SUCCESS;
}

AzureIoTError_t AzureIoTMessagePropertiesFind( AzureIoTMessageProperties_t * messageProperties,
                                               uint8_t * pName,
                                               uint32_t nameLength,
                                               uint8_t ** outValue,
                                               uint32_t * outValueLength )
{
    az_span nameSpan = az_span_create( pName, ( int32_t ) nameLength );
    az_span outValueSpan;
    az_result result;

    if ( ( messageProperties == NULL ) ||
         ( outValue == NULL ) || ( outValueLength == NULL ) )
    {
        AZLogError( ( "AzureIoTMessagePropertiesFind failed: Invalid argument \r\n" ) );
        return AZURE_IOT_INVALID_ARGUMENT;
    }
    
    result = az_iot_message_properties_find( &messageProperties->_internal.properties,
                                             nameSpan, &outValueSpan );

    if( az_result_failed( result ) )
    {
        return AZURE_IOT_STATUS_ITEM_NOT_FOUND;
    }

    *outValue = az_span_ptr( outValueSpan );
    *outValueLength = ( uint32_t ) az_span_size( outValueSpan );

    return AZURE_IOT_SUCCESS;
}

AzureIoTError_t AzureIoTBase64HMACCalculate( AzureIoTGetHMACFunc_t xAzureIoTHMACFunction,
                                             const uint8_t * key_ptr,
                                             uint32_t key_size,
                                             const uint8_t * message_ptr,
                                             uint32_t message_size,
                                             uint8_t * buffer_ptr,
                                             uint32_t buffer_len,
                                             uint8_t ** output_pptr,
                                             uint32_t * output_len_ptr )
{
    AzureIoTError_t status;
    uint8_t * hash_buf;
    uint32_t hash_buf_size = 33;
    char * encoded_hash_buf;
    uint32_t encoded_hash_buf_size = 48;
    uint32_t binary_key_buf_size;

    if ( ( xAzureIoTHMACFunction == NULL ) ||
         ( key_ptr == NULL ) || ( key_size == 0 ) ||
         ( message_ptr == NULL ) || ( message_size == 0 ) ||
         ( buffer_ptr == NULL ) || ( buffer_len == 0 ) ||
         ( output_pptr == NULL ) || ( output_len_ptr == NULL ) )
    {
        AZLogError( ( "AzureIoTBase64HMACCalculate failed: Invalid argument \r\n" ) );
        return AZURE_IOT_INVALID_ARGUMENT;
    }

    binary_key_buf_size = buffer_len;
    status = azure_iot_base64_decode( ( char * ) key_ptr, key_size,
                                      buffer_ptr, binary_key_buf_size, &binary_key_buf_size );

    if( status )
    {
        return status;
    }

    buffer_len -= binary_key_buf_size;

    if( ( hash_buf_size + encoded_hash_buf_size ) > buffer_len )
    {
        return AZURE_IOT_STATUS_OOM;
    }

    hash_buf = buffer_ptr + binary_key_buf_size;

    if( xAzureIoTHMACFunction( buffer_ptr, binary_key_buf_size,
                               message_ptr, ( uint32_t ) message_size,
                               hash_buf, hash_buf_size, &hash_buf_size ) )
    {
        return AZURE_IOT_FAILED;
    }

    hash_buf_size = 33;
    buffer_len -= hash_buf_size;
    encoded_hash_buf = ( char * ) ( hash_buf + hash_buf_size );

    /* Additional space is required by encoder.  */
    hash_buf[ hash_buf_size - 1 ] = 0;
    status = azure_iot_base64_encode( hash_buf, hash_buf_size - 1,
                                      encoded_hash_buf, encoded_hash_buf_size );

    if( status )
    {
        return status;
    }

    *output_pptr = ( uint8_t * ) ( encoded_hash_buf );
    *output_len_ptr =  ( uint32_t ) strlen( encoded_hash_buf );

    return AZURE_IOT_SUCCESS;
}

/*-----------------------------------------------------------*/
