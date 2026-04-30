generate_doxy_docs("ored")

# OREData depends on QuantExt's tag file
add_dependencies(doc_ored doc_quantext)
