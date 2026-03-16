import os
import ORE as ore

startDateString = '2020-05-01'
calendarString = 'Japan'
tenorString = '1M'
businessDayConventionString = 'F'
calAdjXml = os.path.join("..", "..", "Input", "calendaradjustment.xml")

startDate = ore.parseDate(startDateString)
calendar = ore.parseCalendar(calendarString)
tenor =  ore.parsePeriod(tenorString)
bdc = ore.parseBusinessDayConvention(businessDayConventionString)
endOfMonth = False

## ADVANCE DATE -- pre adjustment
endDate = calendar.advance(startDate, tenor, bdc, endOfMonth)
print('---------------------------')
print("advance start date by", tenorString)
print("startDate", startDate.to_date())
print("endDate  ", endDate.to_date())

## EXPLORE QL/ORE JAPAN CALENDAR
print('---------------------------')
print('holidays (pre adjustment):')
holidayList = calendar.holidayList(startDate, endDate)
for idx in holidayList:
    print(idx.to_date())

## MAKE USE OF CALENDAR ADJUSTMENT
## new holiday : 2020-06-01
## new business day (previously holiday) : 2020-05-05
calAdj = ore.CalendarAdjustmentConfig()
calAdj.fromFile(calAdjXml)

print('---------------------------')
print('holidays (post adjustment):')
holidayList = calendar.holidayList(startDate, endDate)
for idx in holidayList:
    print(idx.to_date())

## ADVANCE DATE -- post adjustment
endDate = calendar.advance(startDate, tenor, bdc, endOfMonth)
print('---------------------------')
print("advance start date by", tenorString)
print("startDate", startDate.to_date())
print("endDate  ", endDate.to_date())
print('---------------------------')

mporDays = 3

print(startDate)
#print(ore.calculateMporDate(mporDays))
print(ore.calculateMporDate(mporDays, startDate))
print(ore.calculateMporDate(mporDays, startDate, calendarString))

print(ore.calculateMporDate(mporDays, startDate).to_date())
      