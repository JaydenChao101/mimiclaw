import asyncio
import aiohttp
import time

URL = "http://10.1.14.73:8080/v1/chat/completions"
API_KEY = "sk-123456787654321"

payload = {
    "model": "gpt-oss-20b",
    "messages": [{"role": "user", "content": "Explain mixture of experts briefly"}],
    "max_tokens": 200,
    "temperature": 0.7
}

HEADERS = {
    "Authorization": f"Bearer {API_KEY}",
    "Content-Type": "application/json"
}

async def send_request(session, i):
    start = time.time()

    async with session.post(URL, json=payload, headers=HEADERS) as resp:
        data = await resp.json()

    latency = time.time() - start

    completion_tokens = data.get("usage", {}).get("completion_tokens", 0)

    tps = completion_tokens / latency if latency > 0 else 0

    print(
        f"Request {i} | "
        f"{completion_tokens} tokens | "
        f"{latency:.2f}s | "
        f"{tps:.2f} tok/s"
    )

    return completion_tokens, latency

async def run_test(concurrency=10):
    async with aiohttp.ClientSession() as session:
        tasks = [send_request(session, i) for i in range(concurrency)]

        start = time.time()
        results = await asyncio.gather(*tasks)
        total = time.time() - start

        total_tokens = sum(r[0] for r in results)
        avg_latency = sum(r[1] for r in results) / len(results)

        print("\n===== Result =====")
        print("Concurrency:", concurrency)
        print("Total time:", round(total,2),"s")
        print("Total tokens:", total_tokens)
        print("Avg latency:", round(avg_latency,2),"s")
        print("Overall tokens/sec:", round(total_tokens/total,2))

if __name__ == "__main__":
    asyncio.run(run_test(concurrency=10))