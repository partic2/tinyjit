
/*-------------target---------------*/
//include i386-gen,i386-link
#define TCC_TARGET_I386

//include arm-gen,arm-link
//#define TCC_TARGET_ARM

//include x86_64-gen,x86_64-link
//#define TCC_TARGET_X86_64


/*-------------abi---------------*/
//#define TCC_ARM_EABI
#define TCC_WINDOWS_EABI


/*-------------host---------------*/
//If host and target is in same.
#define TCC_IS_NATIVE
