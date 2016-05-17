#! /usr/bin/env Rscript
#
# Copyright (C) 2016 Quaternion Risk Management Ltd.
# All rights reserved.
#
# don't forget to chmod +x


require(shiny)
require(data.table)

dat = fread("Data/cube.csv")
dat$Date = as.Date(dat$Date)
ins = unique(dat$Id)

server <- function(input, output) {
   output$boxPlot <- renderPlot(
          boxplot(Value ~ DateIndex, dat[Id == input$instr],
                  main = input$instr, border = "black", plot = TRUE)
      )
}

ui <- fluidPage(
   titlePanel("Sanity Check"),
   sidebarPanel(
      selectInput("instr", "Instrument:", ins)
      # ,selectInput("mode", "Show:", list("Plot" = "plot", "Statistics" = "stats", "Data" = "dat"))
   ),
   mainPanel(
      plotOutput("boxPlot")
   )
)

sanity.app = shinyApp(ui = ui, server = server)
runApp(sanity.app, port = 8711, launch.browser = TRUE)
