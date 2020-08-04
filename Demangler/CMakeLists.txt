add_library (Demangler
    "MicrosoftDemangle.cpp"
    "MicrosoftDemangleNodes.cpp"
    "ItaniumDemangle.cpp"
    "include/StringView.h"
    "include/Demangle.h"
    "include/DemangleConfig.h"
    "include/MicrosoftDemangle.h"
    "include/MicrosoftDemangleNodes.h"
    "include/ItaniumDemangle.h"
    "include/Utility.h" "Demangler.cpp" )
target_include_directories (Demangler INTERFACE "include")