SELECT P.EngineProductType [@type],
       P.Model [Model], 
	   (SELECT 
			[name] [@name],
			Parameter [data()] FROM PricingEngineModelParameters  
		WHERE EngineProductType = P.EngineProductType FOR XML PATH('Parameter'), ROOT('ModelParameters'), TYPE),
		P.Engine [Engine],
		(SELECT 
			[name] [@name],
			Parameter [data()] FROM PricingEngineEngineParameters  
		WHERE EngineProductType = P.EngineProductType FOR XML PATH('Parameter'), ROOT('EngineParameters'), TYPE)
FROM PricingEngineProducts P
FOR XML PATH ('Product') , ROOT('PricingEngines')
