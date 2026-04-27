import yfinance as yf
import pandas as pd
import matplotlib as plt

def ingestion_script():
    ticker = yf.Ticker("AAPL")
    df = ticker.history(
        start = "2024-01-01",
        end = "2024-12-31",
        interval = "1d"
    )

    print(f"column {df.columns}")
    df["Close"].plot()















if __name__ == "__main__":
    ingestion_script()