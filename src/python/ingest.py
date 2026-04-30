import yfinance as yf
import pandas as pd
import matplotlib as plt
from pathlib import Path
import sys


def treat_df(df):
    #need to decide how I handle the the Stock Splits
    if "Dividends" in df.columns:
        df.drop(columns = ['Dividends'], inplace = True)
    df.index = df.index.tz_convert("UTC").astype(int)

    #also divide as much as possible by 10 the index, to reduce the size of the index
    t = 10
    while df.index[0]%t == 0:
        t *= 10
    
    t /= 10
    df.index = (df.index/t).astype(int)

    return df


#for now just a very simple ingestion script taking as paremter a stock name and writing the OHLCV of the data in a CSV
def ingestion_script(name):
    name = name.upper()
    try:
        ticker = yf.Ticker(name)
    except Exception as e:
        print(f"Got error {e} when trying to obtain the data of stock {name}")
        return
    
    df = ticker.history(
        start = "2005-01-01",
        end = "2025-12-31",
        interval = "1d"
    )

    df = treat_df(df)

    print(f"column {df.columns}")
    csv_path = f"/home/mael/projects/backtester/src/output/{name}"
    try:
        df.to_csv(csv_path)
    except Exception as e:
        print(f"Got error {e} when trying to write the data to csv {csv_path}")
    else:
        print(f"Successfully wrote {len(df)} rows of data of {name} to {csv_path}")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(f"Usage python3 {sys.argv[0]} <ticker>")
    else:
        name = sys.argv[1]
        ingestion_script(name)