
let DallasESP32 = {
    // Error codes
    DEVICE_DISCONNECTED_C: -127.0,
    DEVICE_DISCONNECTED_F: -196.6,
    DEVICE_DISCONNECTED_RAW: -7040,

    _create: ffi('void* mgos_dallas_create_esp32(int, int, int)'),
    _close: ffi('void mgos_dallas_close(void *)'),
    _begin: ffi('void mgos_dallas_begin(void *)'),
    _gdc: ffi('int mgos_dallas_get_device_count(void *)'),
    _va: ffi('int mgos_dallas_valid_address(void *, char *)'),
    _vf: ffi('int mgos_dallas_valid_family(void *, char *)'),
    _ga: ffi('int mgos_dallas_get_address(void *, char *, int)'),
    _isc: ffi('int mgos_dallas_is_connected(void *, char *)'),
    _iscsp: ffi('int mgos_dallas_is_connected_sp(void *, char *, char *)'),
    _rsp: ffi('int mgos_dallas_read_scratch_pad(void *, char *, char *)'),
    _wsp: ffi('void mgos_dallas_write_scratch_pad(void *, char *, char *)'),
    _rps: ffi('int mgos_dallas_read_power_supply(void *, char *)'),
    _ggr: ffi('int mgos_dallas_get_global_resolution(void *)'),
    _sgr: ffi('void mgos_dallas_set_global_resolution(void *, int)'),
    _gr: ffi('int mgos_dallas_get_resolution(void *, char *)'),
    _sr: ffi('int mgos_dallas_set_resolution(void *, char *, int, int)'),
    _swfc: ffi('void mgos_dallas_set_wait_for_conversion(void *, int)'),
    _gwfc: ffi('int mgos_dallas_get_wait_for_conversion(void *)'),
    _scfc: ffi('void mgos_dallas_set_check_for_conversion(void *, int)'),
    _gcfc: ffi('int mgos_dallas_get_check_for_conversion(void *)'),
    _rts: ffi('void mgos_dallas_request_temperatures(void *)'),
    _rtsba: ffi('int mgos_dallas_request_temperatures_by_address(void *, char *)'),
    _rtsbi: ffi('int mgos_dallas_request_temperatures_by_index(void *, int)'),
    _gt: ffi('int mgos_dallas_get_temp(void *, char *)'),
    _gtc: ffi('int mgos_dallas_get_tempc(void *, char *)'),
    _gtf: ffi('int mgos_dallas_get_tempf(void *, char *)'),
    _gtcbi: ffi('int mgos_dallas_get_tempc_by_index(void *, int)'),
    _gtfbi: ffi('int mgos_dallas_get_tempf_by_index(void *, int)'),
    _isppm: ffi('int mgos_dallas_is_parasite_power_mode(void *)'),
    _iscc: ffi('int mgos_dallas_is_conversion_complete(void *)'),
    _mtwfc: ffi('int mgos_dallas_millis_to_wait_for_conversion(void *, int)'),

    _byte2hex: function (byte) {
        let hex_char = '0123456789abcdef';
        return hex_char[(byte >> 4) & 0x0F] + hex_char[byte & 0x0F];
    },

    // ## **`DallasESP32.create(pin, rmt_rx, rmt_tx)`**
    // Create and return an instance of the dallas temperature: an object with
    // methods described below.
    // `pin` - onewire pin number
    // `rmt_rx` - rx RMT channel
    // `rmt_tx` - tx RMT channel
    //
    // Example:
    // ```javascript
    // let myDT = DallasESP32.create(12 /* onewire pin number */, 0 /*RMT rx channel*/, 1 /*RMT tx channel*/);
    // ```
    create: function (pin, rmt_rx, rmt_tx) {
        let obj = Object.create(DallasESP32._proto);
        // Initialize DallasESP32 library.
        // Return value: handle opaque pointer.
        obj.dt = DallasESP32._create(pin, rmt_rx, rmt_tx);
        DallasESP32._begin(obj.dt);
        return obj;
    },

    _proto: {
        // ## **`myDT.close()`**
        // Close DallasESP32 handle. Return value: none.
        close: function () {
            return DallasESP32._close(this.dt);
        },

        // ## **`myDT.begin()`**
        // Initialise the sensor. Return value: none.
        //begin: function () {
        //    return DallasESP32._begin(this.dt);
        //},

        // ## **`myDT.getDeviceCount()`**
        // Return the number of devices found on the bus.
        // If an operaiton is failed, 0 is returned.
        getDeviceCount: function () {
            return DallasESP32._gdc(this.dt);
        },

        // ## **`myDT.validAddress(addr)`**
        // Check if given onewire `addr` (8-byte string) is valid; returns 1 if it
        // is, or 0 otherwise.
        validAddress: function (addr) {
            return DallasESP32._va(this.dt, addr);
        },

        // ## **`myDT.validFamily(addr)`**
        // Return 1 if onewire address `addr` (8-byte string) is of the family of
        // sensors the lib supports.  Return always 0 if an operaiton failed.
        validFamily: function (addr) {
            return DallasESP32._vf(this.dt, addr);
        },

        // ## **`myDT.getAddress(addr, idx)`**
        // Find an onewire address at a given index `idx` on the bus. Resulting
        // address is written into the provided string buffer `addr`, which should
        // be 8 bytes lont.
        // Return value: 1 in case of success, 0 otherwise.
        // Example:
        // ```javascript
        // load("api_sys.js");
        // load("api_dallas.js");
        //
        // let addr = myDT.createAddress;
        // let res = myDT.getAddress(addr, 0);
        // if (res === 1) {
        //   print("found:", addr);
        // } else {
        //   print("not found");
        // }
        // ```
        getAddress: function (addr, idx) {
            return DallasESP32._ga(this.dt, addr, idx);
        },

        // ## **`myDT.isConnected(addr)`**
        // Determine if the device at the given onewire address (8-byte string) is
        // connected to the bus.
        // Return value: 1 if device is connected, 0 otherwise.
        isConnected: function (addr) {
            return DallasESP32._isc(this.dt, addr);
        },

        // ## **`myDT.isConnectedWithScratchPad(addr, sp)`**
        // Determine if the device at the given onewire address (8-byte string) is
        // connected to the bus, and if so, read the scratch pad to the provided
        // buffer (9-byte string).
        // Return value: 1 if device is connected (and a scratchpad is read), 0
        // otherwise.
        // Example:
        // ```javascript
        // load("api_sys.js");
        // load("api_dallas.js");
        //
        // let sp = myDT.createScratchPad();
        // let res = myDT.isConnectedWithScratchPad("\x28\xff\x2b\x45\x4c\x04\x00\x10", sp);
        // if (res === 1) {
        //   print("connected, scratchpad:", sp);
        // } else {
        //   print("not connected");
        // }
        // ```
        isConnectedWithScratchPad: function (addr, sp) {
            return DallasESP32._iscsp(this.dt, addr, sp);
        },

        // ## **`myDT.readScratchPad(addr, sp)`**
        // Read device's scratchpad.
        // `sp` is a string buffer (minimum 9 bytes length) to read scratchpad
        // into.
        // Return 1 in case of success, 0 otherwise.
        // Example:
        // ```javascript
        // load("api_sys.js");
        // load("api_dallas.js");
        //
        // let sp = createScratchPad();
        // let res = myDT.readScratchPad("\x28\xff\x2b\x45\x4c\x04\x00\x10", sp);
        // if (res === 1) {
        //   print("scratchpad:", sp);
        // } else {
        //   print("failed to read scratchpad");
        // }
        // ```
        readScratchPad: function (addr, sp) {
            return DallasESP32._rsp(this.dt, addr, sp);
        },

        // ## **`myDT.writeScratchPad(addr, sp)`**
        // Write device's scratchpad `sp` (which should be a 9-byte string) by
        // the provided onewire address `addr` (a 8-byte string).
        // Return value: none.
        writeScratchPad: function (addr, sp) {
            return DallasESP32._wsp(this.dt, addr, sp);
        },

        // ## **`myDT.readPowerSupply()`**
        // Read device's power requirements.
        // Return 1 if device needs parasite power.
        // Return always 0 if an operaiton failed.
        readPowerSupply: function (addr) {
            return DallasESP32._rps(this.dt, addr);
        },

        // ## **`myDT.getGlobalResolution()`**
        // Get global resolution in bits. Return value: 9, 10, 11 or 12.
        // In case of a failure, returns 0.
        getGlobalResolution: function () {
            return DallasESP32._ggr(this.dt);
        },

        // ## **`myDT.setGlobalResolution(res)`**
        // Set global resolution `res` in bits, which can be either 9, 10, 11, or
        // 12. If given resolution is out of range, 9 bits is used.
        // Return value: none.
        setGlobalResolution: function (res) {
            return DallasESP32._sgr(this.dt, res);
        },

        // ## **`myDT.getResolution(addr)`**
        // Get device's resolution in bits. Return value: 9, 10, 11 or 12.
        // In case of a failure, returns 0.
        getResolution: function (addr) {
            return DallasESP32._gr(this.dt, addr);
        },

        // ## **`myDT.setResolution(addr, res, skip)`**
        // Set resolution of a device with onewire address `addr` to 9, 10, 11, or
        // 12 bits.  If given resolution is out of range, 9 bits is used.
        // Return 1 in case of success, 0 otherwise.
        setResolution: function (addr, res, skip) {
            return DallasESP32._sr(this.dt, addr, res, skip);
        },

        // ## **`myDT.setWaitForConversion(waitForConversion)`**
        // Set/clear the waitForConversion flag.
        // Return value: none.
        setWaitForConversion: function (f) {
            return DallasESP32._swfc(this.dt, f);
        },

        // ## **`myDT.getWaitForConversion()`**
        // Get the value of the waitForConversion flag: either 1 or 0. In case
        // of a failure, return 0.
        getWaitForConversion: function () {
            return DallasESP32._gwfc(this.dt);
        },

        // ## **`myDT.setCheckForConversion(checkForConversion)`**
        // Set/clear the `checkForConversion` flag.
        setCheckForConversion: function (f) {
            return DallasESP32._scfc(this.dt, f);
        },

        // ## **`myDT.getCheckForConversion()`**
        // Get the value of the `checkForConversion` flag: either 1 or 0. In case
        // of a failure, return 0.
        getCheckForConversion: function () {
            return DallasESP32._gcfc(this.dt);
        },

        // ## **`myDT.requestTemperatures()`**
        // Send command for all devices on the bus to perform a temperature
        // conversion.
        //
        // Return value: 1 in case of success, 0 otherwise.
        requestTemperatures: function () {
            return DallasESP32._rts(this.dt);
        },

        // ## **`myDT.requestTemperaturesByAddress(addr)`**
        // Send command to a device with the given onewire address `addr` to
        // perform a temperature conversion.
        //
        // Return value: 1 in case of success, 0 otherwise.
        requestTemperaturesByAddress: function (addr) {
            return DallasESP32._rtsba(this.dt, addr);
        },

        // ## **`myDT.requestTemperaturesByIndex(idx)`**
        // Send command to a device with the given index `idx` to perform a
        // temperature conversion.
        //
        // Return value: 1 in case of success, 0 otherwise.
        requestTemperaturesByIndex: function (idx) {
            return DallasESP32._rtsbi(this.dt, idx);
        },

        // ## **`myDT.getTemp(addr)`**
        // Return raw temperature value (12 bit integer of 1/128 degrees C)
        // or `DallasESP32.DEVICE_DISCONNECTED_RAW` in case of a failure.
        getTemp: function (addr) {
            // C-functions output value of “1234” equals 12.34 Deg.
            return DallasESP32._gt(this.dt, addr) / 100.0;
        },

        // ## **`myDT.getTempC(addr)`**
        // Returns temperature in degrees C or
        // `DallasESP32.DEVICE_DISCONNECTED_C` in case of a failure.
        getTempC: function (addr) {
            // C-functions output value of “1234” equals 12.34 Deg.
            return DallasESP32._gtc(this.dt, addr) / 100.0;
        },

        // ## **`myDT.getTempF(addr)`**
        // Returns temperature in degrees F or
        // `DallasESP32.DEVICE_DISCONNECTED_F` in case of a failure.
        getTempF: function (addr) {
            // C-functions output value of “1234” equals 12.34 Deg.
            return DallasESP32._gtf(this.dt, addr) / 100.0;
        },

        // ## **`myDT.getTempCByIndex(idx)`**
        // Get temperature from the device with the given index `idx` in degrees C,
        // or `DallasESP32.DEVICE_DISCONNECTED_C` in case of a failure.
        getTempCByIndex: function (idx) {
            // C-functions output value of “1234” equals 12.34 Deg.
            return DallasESP32._gtcbi(this.dt, idx) / 100.0;
        },

        // ## **`myDT.getTempFByIndex(idx)`**
        // Get temperature from the device with the given index `idx` in degrees F,
        // or `DallasESP32.DEVICE_DISCONNECTED_F` in case of a failure.
        getTempFByIndex: function (idx) {
            // C-functions output value of “1234” equals 12.34 Deg.
            return DallasESP32._gtfbi(this.dt, idx) / 100.0;
        },

        // ## **`myDT.isParasitePowerMode()`**
        // Return 1 if the bus requires parasite power, 0 otherwise. In case of a
        // failure return 0.
        isParasitePowerMode: function () {
            return DallasESP32._isppm(this.dt);
        },

        // ## **`myDT.isConversionComplete()`**
        // Return whether a conversion is completed.
        isConversionComplete: function () {
            return DallasESP32._iscc(this.dt);
        },

        // ## **`myDT.millisToWaitForConversion(res)`**
        // Return number of milliseconds to wait until the conversion is completed
        // for the given resolution `res` in bits (9, 10, 11 or 12).
        // In case of a failure, return 0.
        millisToWaitForConversion: function (res) {
            return DallasESP32._mtwfc(this.dt, res);
        },

        // ## **`myDT.toHexStr(addr)`**
        // Return device address `addr` in the hex format.
        toHexStr: function (addr) {
            let res = '';
            for (let i = 0; i < addr.length; i++) {
                res += DallasESP32._byte2hex(addr.charCodeAt(i));
            }
            return res;
        },

        // ## **`myDT.createAddress()`**
        // Return an 8 bytes empty string to be used with `*Address` function
        createAddress: function () {
            return '        ';
        },

        // ## **`myDT.createScratchPad()`**
        // Return an 9 bytes empty string to be used with `...ScratchPad` function
        createScratchPad: function () {
            return '         ';
        }

    },
};
