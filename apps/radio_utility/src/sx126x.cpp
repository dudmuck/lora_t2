#include <cstdint>
#include <sx126x_driver.h>

SX126x::SX126x()
{
}


uint8_t SX126x::setMHz(float MHz)
{
    /*unsigned frf = MHz * MHZ_TO_FRF;
    uint8_t buf[4];

    buf[0] = frf >> 24;
    buf[1] = frf >> 16;
    buf[2] = frf >> 8;
    buf[3] = frf;
    xfer(OPCODE_SET_RF_FREQUENCY, 4, 0, buf);
    return buf[3];*/
	//yyy TODO
	return 0;
}

float SX126x::getMHz()
{
    /*uint32_t frf = readReg(REG_ADDR_RFFREQ, 4);
    return frf / (float)MHZ_TO_FRF;*/
	// yyy TODO
	return 0;
}
