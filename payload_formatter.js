function decodeUplink(input) {
    var decoded = {};
  
    decoded.temperature = (((input.bytes[0]<<8)>>>0) + input.bytes[1])/100;
    decoded.humidity = (((input.bytes[2]<<8)>>>0) + input.bytes[3])/100;
    decoded.co_we = (((input.bytes[4]<<8)>>>0) + input.bytes[5])/10000;
    decoded.co_ae = (((input.bytes[6]<<8)>>>0) + input.bytes[7])/10000;
    decoded.no2_we = (((input.bytes[8]<<8)>>>0) + input.bytes[9])/10000;
    decoded.no2_ae = (((input.bytes[10]<<8)>>>0) + input.bytes[11])/10000;
    decoded.ox_we = (((input.bytes[12]<<8)>>>0) + input.bytes[13])/10000;
    decoded.ox_ae = (((input.bytes[14]<<8)>>>0) + input.bytes[15])/10000;
      
      return {
          data: {
              temperature: decoded.temperature,
              humidity: decoded.humidity,
              co_we: decoded.co_we,
              co_ae: decoded.co_ae,
              no2_we: decoded.no2_we,
              no2_ae: decoded.no2_ae,
              ox_we: decoded.ox_we,
              ox_ae: decoded.ox_ae,
          },
          warnings: [],
          errors: []
      };
  }