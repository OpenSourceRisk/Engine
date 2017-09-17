USE ORE

INSERT INTO TypesCurrencyCode (value) VALUES ('default')
INSERT INTO TypesCurrencyPair (value) VALUES ('default')
-- this is ugly, but necessary for XML import (and multiple calendar choices). maybe in the future we get an relation between Typescalendar and TypesMulticalendar...
-- add calendar combinations as needed for XML import or selection...
INSERT Typescalendar (value) VALUES ('TARGET,US,UK')
INSERT Typescalendar (value) VALUES ('UK,US')
INSERT Typescalendar (value) VALUES ('UK,TARGET')
INSERT Typescalendar (value) VALUES ('ZUB,TARGET,UK')
INSERT Typescalendar (value) VALUES ('ZUB,US,UK')
INSERT Typescalendar (value) VALUES ('ZUB,US')
INSERT Typescalendar (value) VALUES ('TARGET,ZUB')
INSERT Typescalendar (value) VALUES ('TARGET,UK')
INSERT Typescalendar (value) VALUES ('TARGET,US')
INSERT Typescalendar (value) VALUES ('ZUB,UK')
INSERT Typescalendar (value) VALUES ('JP,UK')
INSERT Typescalendar (value) VALUES ('US,UK')
