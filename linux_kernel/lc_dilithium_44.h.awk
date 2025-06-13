# Set the Dilithium security strength
{gsub("@dilithium_strength@", "2", $0) ;
 gsub("@dilithium_name@", "lc_dilithium_44", $0) ;
 gsub("@dilithium_macro@", "LC_DILITHIUM_44", $0) ;
 gsub("@dilithium_header@", "44_", $0) ;

 # Define LC_DILITHIUM_ED25519_SIG, LC_DILITHIUM_ED448_SIG
 # Comment out if CONFIG_LEANCRYPTO_DILITHIUM_ED25519,
 # CONFIG_LEANCRYPTO_DILITHIUM_ED448 is unset
 gsub("mesondefine", "define", $0) ;

 print $0}
