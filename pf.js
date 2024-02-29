function decodeUplink(input) {    
    var decoded = {};

    const ajuste_temp = {
        "co": [
            [0.70, 0.70, 0.70, 0.70, 1.00, 3.00, 3.50, 4.00, 4.50],
            [0.20, 0.20, 0.20, 0.20, 0.30, 1.00, 1.20, 1.30, 1.50],
            [-1.00, -0.50, 0.00, 0.00, 0.00, 1.00, 1.00, 1.00, 1.00],
            [55.00, 55.00, 55.00, 50.00, 31.00, 0.00, -50.00, -150.00, -250.00]
        ],
        "no2": [
            [1.30, 1.30, 1.30, 1.30, 1.00, 0.60, 0.40, 0.20, -1.50],
            [2.20, 2.20, 2.20, 2.20, 1.70, 1.00, 0.70, 0.30, -2.50],
            [1.00, 1.00, 1.00, 1.00, 1.00, 1.00, 0.40, -0.10, -4.00],
            [7.00, 7.00, 7.00, 7.00, 4.00, 0.00, 0.50, 5.00, 67.00]
        ],
        "ox": [
            [0.90, 0.90, 1.00, 1.30, 1.50, 1.70, 2.00, 2.50, 3.70],
            [0.50, 0.50, 0.60, 0.80, 0.90, 1.00, 1.20, 1.50, 2.20],
            [0.50, 0.50, 0.50, 0.60, 0.60, 1.00, 2.80, 5.00, 5.30],
            [1.00, 1.00, 1.00, 1.00, 1.00, 1.00, 8.50, 23.00, 103.00]
        ],
        "so2": [
            [1.60, 1.60, 1.60, 1.60, 1.60, 1.60, 1.90, 3.00, 5.80],
            [1.00, 1.00, 1.00, 1.00, 1.00, 1.00, 1.20, 1.90, 3.60],
            [1.00, 1.00, 1.00, 1.00, 1.00, 1.00, 2.00, 3.50, 7.00],
            [-4.00, -4.00, -4.00, -4.00, -4.00, 0.00, 20.00, 140.00, 450.00]
        ]
      };

      const sensors = {
        //   0,       1,        2,     3     ,   4        ,   5          , 6            , 7 
        //gain,  we_zero,  ae_zero, we_sensor, sensitivity, electronic_we, electronic_ae, no_sensitivity
        "co": [0.8, 353, 328, 454, 0.363, 343, 328, 0],
        "ox": [-0.73, 229, 234, -506, 0.369, 237, 242, -587],
        "no2":[-0.73, 222, 212, -424, 0.31, 230, 220, 0],
        "so2": [0.8, 361, 350, 363, 0.29, 335, 343, 0]
      };
      

    const variables = ["CO", "NO2", 'SO2', 'OX'];
    const int_var = ["Temperatura_1", "Umidade_1", "MP-1_1", "MP-2_5_1", "MP-10_1"];
    
    const suffixes = ["_we", "_ae"];
    
    const NUM_VARIABLES = 4; // Number of variables per type (h2s, nh3)
    const FLOAT_SIZE = 4; // Size of a float in bytes
    const INT_SIZE = 2; // Size of an integer in bytes
    const FLOAT_OFFSET = 0; // Start index of the floats in the buffer
    const INT_OFFSET = NUM_VARIABLES * FLOAT_SIZE * variables.length + 4 ; // Start index of the integers in the buffer
    const TEMP_CONVERSION_FACTOR = 10.0; // Conversion factor for floats

        //   0,       1,        2,     3     ,   4        ,   5          , 6            , 7 
        //gain,  we_zero,  ae_zero, we_sensor, sensitivity, electronic_we, electronic_ae, no_sensitivity

    function algorithm1(raw_we, raw_ae, temp, model) {
        const kt = ajuste_temp[model][0]
        const t = kt[Math.floor(temp / 10) + 3];    
        //console.log(kt)
        //console.log(t)
        //console.log(this.model)
        //return (raw_we - electronic_we) - t * (raw_ae - electronic_ae);           sensitivity
        return ((raw_we - sensors[model][5]) - t * (raw_ae - sensors[model][6])) / sensors[model][4];
    }
  
    function  algorithm2(raw_we, raw_ae, temp, model) {
        const kt = ajuste_temp[model][1]
        const t = kt[Math.floor(temp / 10) + 3];
        //return (raw_we - this.electronic_we) - (this.we_zero / this.ae_zero) * t * (raw_ae - this.electronic_ae);
        return ((raw_we - sensors[model][5]) - (sensors[model][1] / sensors[model][2]) * t * (raw_ae - sensors[model][6]))/sensors[model][4];
      }
  
    function  algorithm3(raw_we, raw_ae, temp, model) {
        const kt = ajuste_temp[model][2]
        const t = kt[Math.floor(temp / 10) + 3];
        //return (raw_we - this.electronic_we) - (this.we_zero - this.ae_zero) - t * (raw_ae - this.electronic_ae);
        return ((raw_we - sensors[model][5]) - (sensors[model][1] / sensors[model][2]) - t * (raw_ae - sensors[model][6]))/sensors[model][4];
      }
  
    function algorithm4(raw_we, temp, model) {
        const kt = ajuste_temp[model][3]
        const t = kt[Math.floor(temp / 10) + 3];
        //return (raw_we - this.electronic_we - this.we_zero - t);
        return ((raw_we -  sensors[model][5]-  sensors[model][1] - t))/ sensors[model][4];
      }
  

    function getFloat32(bytes, index) {
        var arrayBuffer = new ArrayBuffer(4);
        var byteView = new Uint8Array(arrayBuffer);
        for (var i = 0; i < 4; i++) {
            byteView[i] = bytes[index + i];
        }
        var dataView = new DataView(arrayBuffer);
        return dataView.getFloat32(0, true); // little-endian
    }

    function getUint16(bytes, index) {
        return (bytes[index+1] << 8) | bytes[index];
    }

    function getUint64(bytes, index) {
        var value = 0;
        for (var i = 0; i < 8; i++) {
            value += (bytes[index + i] * Math.pow(256, i));
        }
        return value;
    }

    function getBestValue(ppbArray) {
        // This function needs to be defined to calculate the Best value
        // based on the criteria you want to use (average, median, etc.).
        // For now, it just returns the average.
        
        // Filter out negative values before calculating the average
        let positiveValues = ppbArray.filter(value => value > 0);
        // If no positive values are present, return 0 or some error code
        if (positiveValues.length === 0) return null; // or some other indication of no valid data
        // Calculate the average of the positive values
    
        let sum = positiveValues.reduce((a, b) => a + b, 0);
        return sum / positiveValues.length;
    }


    if (input.fPort === 2)        {
        // single readings const int_var = ["Temperatura_1", "Umidade_1", "MP-1_1", "MP-2_5_1", "MP-10_1"];        
        offset = variables.length * 2 * FLOAT_SIZE;
        decoded['Anemometro_1']= getFloat32(input.bytes, offset);        
        offset += FLOAT_SIZE;

        temp= getUint16(input.bytes, offset)/TEMP_CONVERSION_FACTOR;
        decoded['Temperatura_1'] =temp;
        offset += 2;
        umid =  getUint16(input.bytes, offset)/TEMP_CONVERSION_FACTOR;
        decoded['Umidade_1'] = umid;
        offset += 2;

        // Particulate matter readings
        decoded['MP-1_1'] = getUint16(input.bytes, offset);
        offset += 2;
        decoded['MP-25_1'] = getUint16(input.bytes, offset);
        offset += 2;
        decoded['MP-10_1'] = getUint16(input.bytes, offset);
        offset += 2;    

        offset = 0
        let co_we = getFloat32(input.bytes, offset);
        offset += FLOAT_SIZE;
        let co_ae = getFloat32(input.bytes, offset);
        offset += FLOAT_SIZE;
        decoded['CO_WE']= co_we;
        decoded['CO_AE'] = co_ae;
        decoded['CO_1']= algorithm1(co_we, co_ae, temp, 'co');
        decoded['CO_2']= algorithm2(co_we, co_ae, temp, 'co');
        decoded['CO_3']= algorithm3(co_we, co_ae, temp, 'co');
        decoded['CO_4']= algorithm4(co_we, temp, 'co');
        let ppbArray = [];
        ppbArray.push(decoded['CO_1']);
        ppbArray.push(decoded['CO_2']);
        ppbArray.push(decoded['CO_3']);
        ppbArray.push(decoded['CO_4']);
        decoded['Best_CO']= getBestValue(ppbArray);
        // NO2
        let no2_we = getFloat32(input.bytes, offset);
        offset += FLOAT_SIZE;
        let no2_ae = getFloat32(input.bytes, offset);
        offset += FLOAT_SIZE;
        decoded['NO2_WE']= no2_we;
        decoded['NO2_AE'] = no2_ae;
        decoded['NO2_1']= algorithm1(no2_we, no2_ae, temp, 'no2');
        decoded['NO2_2']= algorithm2(no2_we, no2_ae, temp, 'no2');
        decoded['NO2_3']= algorithm3(no2_we, no2_ae, temp, 'no2');
        decoded['NO2_4']= algorithm4(no2_we, temp, 'no2');
        ppbArray = [];
        ppbArray.push(decoded['NO2_1']);
        ppbArray.push(decoded['NO2_2']);
        ppbArray.push(decoded['NO2_3']);
        ppbArray.push(decoded['NO2_4']);
        decoded['Best_NO2']= getBestValue(ppbArray);
        
        
        let so2_we = getFloat32(input.bytes, offset);
        offset += FLOAT_SIZE;
        let so2_ae = getFloat32(input.bytes, offset);
        offset += FLOAT_SIZE;
        decoded['SO2_WE']= so2_we;
        decoded['SO2_AE'] = so2_ae;
        decoded['SO2_1']= algorithm1(so2_we, so2_ae, temp, 'so2');
        decoded['SO2_2']= algorithm2(so2_we, so2_ae, temp, 'so2');
        decoded['SO2_3']= algorithm3(so2_we, so2_ae, temp, 'so2');
        decoded['SO2_4']= algorithm4(so2_we, temp, 'no2');
        ppbArray = [];
        ppbArray.push(decoded['SO2_1']);
        ppbArray.push(decoded['SO2_2']);
        ppbArray.push(decoded['SO2_3']);
        ppbArray.push(decoded['SO2_4']);
        decoded['Best_SO2']= getBestValue(ppbArray);

 
 
        let ox_we = getFloat32(input.bytes, offset);
        offset += FLOAT_SIZE;
        let ox_ae = getFloat32(input.bytes, offset);
        offset += FLOAT_SIZE;
        decoded['OX_WE']= ox_we;
        decoded['OX_AE'] = ox_ae;
        
        if (decoded['NO2_1'] < 0){
          no2= 0;
        } else {
          no2= decoded[NO2_1];
        }
        adjustedWe = ox_we - no2 *  sensors['ox'][4]* sensors['ox'][0];
        
        decoded['OX_1']= algorithm1(adjustedWe, ox_ae, temp, 'ox');
        decoded['OX_2']= algorithm2(adjustedWe, ox_ae, temp, 'ox');
        decoded['OX_3']= algorithm3(adjustedWe, ox_ae, temp, 'ox');
        decoded['OX_4']= algorithm4(adjustedWe, temp, 'ox');
        ppbArray = [];
        ppbArray.push(decoded['OX_1']);
        ppbArray.push(decoded['OX_2']);
        ppbArray.push(decoded['OX_3']);
        ppbArray.push(decoded['OX_4']);
        decoded['Best_OX']= getBestValue(ppbArray);
        
        
    }     
        
    return {
        data: decoded,
        warnings: [],
        errors: []
    };
}